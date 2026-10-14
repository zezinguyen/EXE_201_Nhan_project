#include "IrLearner.h"
#include "AudioPlayer.h"
#include "DisplayOled.h"
#include "StatusLed.h"
#include "PowerManager.h"
#include "SupabaseRestClient.h"

IrLearner::IrLearner() 
    : irRecv(PIN_IR_RECV_OUT, 1024, 50, true) {}

void IrLearner::begin() {
    pinMode(PIN_IR_RECV_VCC, OUTPUT);
    powerSensor(false); // Ban đầu tắt nguồn mắt thu để tiết kiệm pin (Spec 3.2, 5.3)
}

void IrLearner::powerSensor(bool on) {
    digitalWrite(PIN_IR_RECV_VCC, on ? HIGH : LOW);
}

void IrLearner::startLearning(LearnOrigin origin, const String& requestId) {
    active = true;
    currentOrigin = origin;
    currentRequestId = requestId;
    step = 1;
    mismatchCount = 0;
    startTime = millis();

    powerSensor(true);
    irRecv.enableIRIn();

    StatusLed::instance().setPattern(LED_LEARNING);
    AudioPlayer::instance().playTrack(TRACK_PROMPT_REMOTE);
    DisplayOled::instance().showLearningPrompt(1);
    PowerManager::instance().wakeScreen();

    Serial.printf("[LEARN] Bắt đầu học lệnh (Origin=%s, RequestId=%s)\n", 
        origin == LEARN_FROM_APP ? "APP" : "BUTTON", requestId.c_str());
}

void IrLearner::cancelLearning() {
    if (!active) return;
    active = false;
    irRecv.disableIRIn();
    powerSensor(false);

    StatusLed::instance().setPattern(LED_ONLINE);
    DisplayOled::instance().showStatus("Online", "Da huy hoc lenh");
    Serial.println("[LEARN] Đã huỷ chế độ học lệnh.");
}

bool IrLearner::compareCaptures(const IrCode& a, const IrCode& b) {
    if (!a.isRaw() && !b.isRaw()) {
        return (a.protocol == b.protocol && a.code == b.code);
    }
    // So khớp mã RAW (cho phép lệch ±20%)
    if (a.raw.size() != b.raw.size()) return false;
    for (size_t i = 0; i < a.raw.size(); i++) {
        int diff = abs((int)a.raw[i] - (int)b.raw[i]);
        if (diff > (a.raw[i] * 0.25f + 50)) return false;
    }
    return true;
}

void IrLearner::finishSuccess(const IrCode& code) {
    active = false;
    irRecv.disableIRIn();
    powerSensor(false);

    StatusLed::instance().setPattern(LED_ONLINE);
    AudioPlayer::instance().playTrack(TRACK_LEARN_SUCCESS);
    DisplayOled::instance().showTemporaryMessage("Hoc thanh cong!", "Da ghi nhan ma IR", 3000);

    Serial.printf("[LEARN] Học thành công! Protocol=%s, Code=%s\n", 
        code.protocol.c_str(), code.code.c_str());

    if (currentOrigin == LEARN_FROM_APP) {
        // Gửi kết quả lên Supabase qua RPC board_learn_event
        SupabaseRestClient::instance().sendLearnEvent("learn_result", currentRequestId, "ok", &code);
    } else {
        // Gửi mã vào bảng learned_codes để chủ nhà gán trong app
        SupabaseRestClient::instance().queueLearnedCode(code);
    }
}

void IrLearner::finishFailure(const String& reason) {
    active = false;
    irRecv.disableIRIn();
    powerSensor(false);

    StatusLed::instance().setPattern(LED_ONLINE);
    AudioPlayer::instance().playTrack(TRACK_LEARN_FAIL);
    DisplayOled::instance().showTemporaryMessage("Hoc that bai!", reason, 3000);

    Serial.printf("[LEARN] Thất bại: %s\n", reason.c_str());

    if (currentOrigin == LEARN_FROM_APP) {
        String eventName = (reason == "timeout") ? "learn_timeout" : "learn_result";
        String status = (reason == "timeout") ? "timeout" : "E_LEARN_FAIL";
        SupabaseRestClient::instance().sendLearnEvent(eventName, currentRequestId, status, nullptr);
    }
}

void IrLearner::update() {
    if (!active) return;

    unsigned long now = millis();

    // Kiểm tra timeout 15 giây (Spec 7.2)
    if (now - startTime >= LEARN_TIMEOUT_MS) {
        finishFailure("timeout");
        return;
    }

    if (irRecv.decode(&results)) {
        IrCode captured;
        if (results.decode_type != decode_type_t::UNKNOWN) {
            captured.protocol = typeToString(results.decode_type);
            captured.bits = results.bits;
            char hexBuf[20];
            snprintf(hexBuf, sizeof(hexBuf), "0x%llX", results.value);
            captured.code = String(hexBuf);
        } else {
            captured.protocol = "UNKNOWN";
            captured.freq = 38;
            for (uint16_t i = 1; i < results.rawlen; i++) {
                captured.raw.push_back(results.rawbuf[i] * kRawTick);
            }
        }

        Serial.printf("[LEARN] Thu được tín hiệu lần %d: %s %s\n", 
            step, captured.protocol.c_str(), captured.code.c_str());

        if (step == 1) {
            firstCapture = captured;
            step = 2;
            startTime = now; // Reset lại 15s cho lần bấm thứ 2
            DisplayOled::instance().showLearningPrompt(2);
            PowerManager::instance().wakeScreen();

            if (currentOrigin == LEARN_FROM_APP) {
                SupabaseRestClient::instance().sendLearnEvent("learn_progress", currentRequestId, "first_captured", nullptr);
            }
        } else if (step == 2) {
            if (compareCaptures(firstCapture, captured)) {
                finishSuccess(firstCapture);
            } else {
                mismatchCount++;
                Serial.printf("[LEARN] 2 lần bấm không khớp! (Lệch %d/3)\n", mismatchCount);
                if (mismatchCount >= 3) {
                    finishFailure("mismatch_limit");
                } else {
                    step = 1; // Bắt đầu lại từ lần 1
                    DisplayOled::instance().showLearningPrompt(1, "Khong khop, thu lai");
                    if (currentOrigin == LEARN_FROM_APP) {
                        SupabaseRestClient::instance().sendLearnEvent("learn_progress", currentRequestId, "mismatch", nullptr);
                    }
                }
            }
        }

        irRecv.resume();
    }
}

