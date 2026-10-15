#include "CommandHandler.h"

#include <Arduino.h>

#include "config.h"

namespace {
constexpr const char* kErrBusy = "E_BUSY";
constexpr const char* kErrRateLimited = "E_RATE_LIMITED";
constexpr const char* kErrBadRequest = "E_BAD_REQUEST";

const char* nameOf(ErrorCode code) { return errorCodeName(code); }
}  // namespace

CommandHandler::CommandHandler(DeviceStore& store, IrSender& sender, IrLearner& learner, ErrorReporter& errors,
                               TimeSync& clock, RealtimeClient& realtime, Outbox& outbox)
    : store_(store),
      sender_(sender),
      learner_(learner),
      errors_(errors),
      clock_(clock),
      realtime_(realtime),
      outbox_(outbox),
      guard_(COMMAND_RATE_MAX, COMMAND_RATE_WINDOW_MS) {}

void CommandHandler::onBroadcast(const char* event, JsonObjectConst payload) {
    if (strcmp(event, "command") == 0) {
        handleCommand(payload);
    } else if (strcmp(event, "learn_start") == 0) {
        learnRequestId_ = payload["requestId"] | "";
        startLearning(LearnSource::App, millis());
    } else if (strcmp(event, "learn_cancel") == 0) {
        learner_.stop();
    }
    // TODO: "profile_updated" -> so profileVersion, tải profile mới qua HTTPS rồi store_.save()
}

LearnEvent CommandHandler::update(uint32_t nowMs) {
    const LearnEvent event = learner_.update(nowMs);
    switch (event) {
        case LearnEvent::None:
            break;
        case LearnEvent::FirstCaptured:
            sendLearnEvent("learn_progress", "first_captured");
            break;
        case LearnEvent::Mismatch:
            sendLearnEvent("learn_progress", "mismatch");
            break;
        case LearnEvent::Failed:
            errors_.report(ErrorCode::LearnFail, nullptr, clock_.nowEpochSec());
            sendLearnEvent("learn_result", nameOf(ErrorCode::LearnFail));
            break;
        case LearnEvent::Success:
            finishLearning();
            break;
        case LearnEvent::Timeout:
            sendLearnEvent("learn_timeout", "timeout");
            break;
    }
    return event;
}

void CommandHandler::startLearning(LearnSource source, uint32_t nowMs) {
    if (source == LearnSource::Button) {
        learnRequestId_ = "";
    }
    learnSource_ = source;
    learner_.start(nowMs);
}

const char* CommandHandler::runAction(const char* deviceId, const char* action, JsonObjectConst params) {
    if (learner_.isActive()) {
        return kErrBusy;  // Đang học lệnh thì không phát IR
    }
    JsonObjectConst device = store_.findDevice(deviceId);
    const char* error = device.isNull() ? nameOf(ErrorCode::CmdNotFound) : sendDeviceCommand(device, action, params);

    ErrorCode code;
    if (error != nullptr && errorCodeFromName(error, code)) {
        errors_.report(code, deviceId, clock_.nowEpochSec());
    }
    return error;
}

void CommandHandler::handleCommand(JsonObjectConst command) {
    const char* commandId = command["commandId"];
    const char* deviceId = command["deviceId"];
    const char* action = command["action"];
    if (commandId == nullptr || deviceId == nullptr || action == nullptr) {
        sendAck(commandId, kErrBadRequest);
        return;
    }

    const CommandVerdict verdict = guard_.check(commandId, command["expiresAt"] | 0LL, command["seq"] | 0u,
                                                clock_.nowEpochMs(), clock_.isSynced(), millis());
    switch (verdict) {
        case CommandVerdict::Expired:
        case CommandVerdict::StaleSequence:
            log_i("Bo qua lenh %s: %s", commandId, commandVerdictName(verdict));
            return;  // Lệnh cũ: bỏ qua, không ACK
        case CommandVerdict::Duplicate: {
            // App thử lại: ACK lại đúng kết quả lần đầu, không phát IR lần 2
            const char* previous = nullptr;
            if (guard_.findResult(commandId, previous)) {
                sendAck(commandId, previous, true);
            }
            return;
        }
        case CommandVerdict::RateLimited:
            sendAck(commandId, kErrRateLimited);
            return;
        case CommandVerdict::Accept:
            break;
    }
    const char* error = runAction(deviceId, action, command["params"]);
    guard_.setResult(commandId, error);
    sendAck(commandId, error);
    if (resultHandler_ != nullptr) {
        resultHandler_(commandId, error);
    }
}

void CommandHandler::sendAck(const char* commandId, const char* error, bool duplicate) {
    JsonDocument ack;
    ack["commandId"] = commandId;
    ack["ok"] = error == nullptr;
    if (error != nullptr) {
        ack["error"] = error;
    }
    if (duplicate) {
        ack["duplicate"] = true;
    }
    log_i("ACK %s: %s%s", commandId != nullptr ? commandId : "-", error != nullptr ? error : "ok",
          duplicate ? " (trung)" : "");
    realtime_.broadcast("ack", ack);
}

void CommandHandler::sendLearnEvent(const char* event, const char* status) {
    if (learnSource_ != LearnSource::App) {
        return;  // Học từ nút: không có màn hình app nào đang chờ
    }
    JsonDocument payload;
    payload["requestId"] = learnRequestId_;
    payload["status"] = status;
    realtime_.broadcast(event, payload);
}

void CommandHandler::finishLearning() {
    const IrCapture& capture = learner_.result();
    if (learnSource_ == LearnSource::Button) {
        const uint32_t epochSec = clock_.isSynced() ? clock_.nowEpochSec() : 0;
        if (!outbox_.queueLearnedCode(capture, epochSec)) {
            log_w("Hang doi ma hoc day, da bo ma cu nhat");
        }
        return;
    }
    JsonDocument payload;
    payload["requestId"] = learnRequestId_;
    payload["status"] = "ok";
    irCaptureToJson(capture, payload["ir"].to<JsonObject>());
    realtime_.broadcast("learn_result", payload);
}

const char* CommandHandler::sendDeviceCommand(JsonObjectConst device, const char* action, JsonObjectConst params) {
    // Điều hòa: gửi cả trạng thái qua IRac
    const char* acProtocol = device["irac_protocol"];
    if (acProtocol != nullptr) {
        const int16_t tempMin = device["limits"]["temp_min"] | AC_TEMP_MIN_DEFAULT;
        const int16_t tempMax = device["limits"]["temp_max"] | AC_TEMP_MAX_DEFAULT;
        AcRequest request;
        request.protocol = acProtocol;
        request.power = strcmp(action, "POWER_OFF") != 0 && (params["power"] | true);
        request.temperatureC = constrain(params["temp"] | 24, tempMin, tempMax);
        request.mode = params["mode"] | "cool";
        request.fan = params["fan"] | "auto";
        // TODO: lưu trạng thái gần nhất vào NVS (spec 11)
        return sender_.sendAc(request) ? nullptr : nameOf(ErrorCode::IrSendFail);
    }

    // Thiết bị "bấm nút": tra mã theo action
    JsonObjectConst command = device["commands"][action];
    if (command.isNull()) {
        return nameOf(ErrorCode::CmdNotFound);
    }

    JsonArrayConst raw = command["raw"];
    if (!raw.isNull()) {
        static uint16_t buffer[IR_RAW_MAX_LENGTH];
        uint16_t length = 0;
        for (JsonVariantConst value : raw) {
            if (length >= IR_RAW_MAX_LENGTH) {
                return nameOf(ErrorCode::ProfileInvalid);
            }
            buffer[length++] = value.as<uint16_t>();
        }
        return sender_.sendRaw(buffer, length, command["freq"] | IR_DEFAULT_KHZ) ? nullptr
                                                                                : nameOf(ErrorCode::IrSendFail);
    }

    const char* protocol = command["protocol"];
    const char* code = command["code"];
    if (protocol == nullptr || code == nullptr) {
        return nameOf(ErrorCode::ProfileInvalid);
    }
    const uint64_t value = strtoull(code, nullptr, 0);
    return sender_.sendCode(protocol, value, command["bits"] | 0) ? nullptr : nameOf(ErrorCode::IrSendFail);
}
