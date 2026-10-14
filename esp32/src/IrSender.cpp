#include "IrSender.h"
#include "DeviceStore.h"
#include "AudioPlayer.h"
#include "DisplayOled.h"
#include "IrLearner.h"
#include <time.h>

IrSender::IrSender() 
    : irSend(PIN_IR_SEND), irAc(PIN_IR_SEND) {}

void IrSender::begin() {
    irSend.begin();
}

bool IrSender::isDuplicate(const String& cmdId) {
    for (const String& id : recentCommandIds) {
        if (id == cmdId) return true;
    }
    return false;
}

void IrSender::recordCommand(const String& cmdId) {
    if (recentCommandIds.size() >= MAX_DEDUP_CACHE_SIZE) {
        recentCommandIds.erase(recentCommandIds.begin());
    }
    recentCommandIds.push_back(cmdId);
}

bool IrSender::isRateLimited() {
    uint32_t now = millis();
    uint8_t oldestIndex = (rateIndex + 1) % MAX_COMMANDS_PER_SECOND;
    if (now - commandTimestamps[oldestIndex] < 1000) {
        return true; // Quá 5 lệnh trong 1 giây
    }
    rateIndex = (rateIndex + 1) % MAX_COMMANDS_PER_SECOND;
    commandTimestamps[rateIndex] = now;
    return false;
}

SendResult IrSender::handleCommand(
    const String& commandId,
    const String& deviceId,
    const String& action,
    const JsonObjectConst& params,
    uint64_t expiresAt,
    uint32_t seq
) {
    SendResult res;

    // 1. Kiểm tra tham số bắt buộc (Contract 3.2)
    if (commandId.isEmpty() || deviceId.isEmpty() || action.isEmpty()) {
        res.ok = false;
        res.error = "E_BAD_REQUEST";
        return res;
    }

    // 2. Chống lệnh trùng (Idempotency)
    if (isDuplicate(commandId)) {
        Serial.printf("[IR] Phát hiện lệnh trùng commandId=%s, trả ACK cũ\n", commandId.c_str());
        res.ok = true;
        res.duplicate = true;
        return res;
    }

    // 3. Nếu mạch đang trong phiên học lệnh -> Báo E_BUSY (Contract 3.4 & 3.5)
    if (IrLearner::instance().isLearning()) {
        Serial.printf("[IR] Mạch đang học lệnh, từ chối phát IR (E_BUSY)\n");
        res.ok = false;
        res.error = "E_BUSY";
        AudioPlayer::instance().playTrack(TRACK_CMD_FAILED);
        return res;
    }

    // 4. Kiểm tra quá hạn expiresAt theo giờ NTP hoặc seq khi chưa có NTP (Contract 3.2)
    time_t nowSec = time(nullptr);
    if (nowSec > 1700000000) { // Đã có giờ NTP
        uint64_t nowMs = (uint64_t)nowSec * 1000;
        if (expiresAt > 0 && nowMs > expiresAt) {
            Serial.printf("[IR] Lệnh %s đã quá hạn (now=%llu > expiresAt=%llu), bỏ qua\n", 
                commandId.c_str(), nowMs, expiresAt);
            res.ok = false;
            res.error = "E_EXPIRED";
            return res; // Không gửi ACK theo Contract 3.2
        }
    } else { // Chưa có giờ NTP: chỉ nhận lệnh có seq > lệnh gần nhất
        if (seq <= lastSeenSeq && lastSeenSeq > 0) {
            Serial.printf("[IR] Chưa có NTP, bỏ qua lệnh seq cũ (seq=%u <= last=%u)\n", seq, lastSeenSeq);
            res.ok = false;
            res.error = "E_EXPIRED";
            return res;
        }
        lastSeenSeq = seq;
    }

    // 5. Giới hạn tốc độ phát IR (Rate limiting)
    if (isRateLimited()) {
        Serial.printf("[IR] Bị giới hạn tốc độ: quá 5 lệnh/giây\n");
        res.ok = false;
        res.error = "E_RATE_LIMITED";
        AudioPlayer::instance().playTrack(TRACK_CMD_FAILED);
        return res;
    }

    // 6. Kiểm tra thiết bị có trong profile không
    if (!DeviceStore::instance().hasDevice(deviceId)) {
        Serial.printf("[IR] Không tìm thấy thiết bị %s trong profile\n", deviceId.c_str());
        res.ok = false;
        res.error = "E_CMD_NOT_FOUND";
        AudioPlayer::instance().playTrack(TRACK_CMD_NOT_FOUND);
        return res;
    }

    // 6. Phát tín hiệu IR theo loại thiết bị
    String acProtocol;
    int tempMin = 16, tempMax = 30;
    bool sendOk = false;

    if (DeviceStore::instance().getAcConfig(deviceId, acProtocol, tempMin, tempMax)) {
        // Thiết bị là điều hòa (IRac)
        sendOk = sendAcCommand(acProtocol, params, tempMin, tempMax);
    } else {
        // Thiết bị phím bấm (TV, quạt...)
        IrCode code;
        if (!DeviceStore::instance().findIrCode(deviceId, action, code)) {
            Serial.printf("[IR] Không tìm thấy nút %s của thiết bị %s trong profile\n", 
                action.c_str(), deviceId.c_str());
            res.ok = false;
            res.error = "E_CMD_NOT_FOUND";
            AudioPlayer::instance().playTrack(TRACK_CMD_NOT_FOUND);
            return res;
        }
        sendOk = sendIrByCode(code);
    }

    if (!sendOk) {
        res.ok = false;
        res.error = "E_IR_SEND_FAIL";
        AudioPlayer::instance().playTrack(TRACK_CMD_FAILED);
        return res;
    }

    // Thành công
    recordCommand(commandId);
    res.ok = true;
    AudioPlayer::instance().playTrack(TRACK_CMD_SUCCESS);
    DisplayOled::instance().showTemporaryMessage("Lenh IR thanh cong", action, 2000);
    return res;
}

bool IrSender::sendIrByCode(const IrCode& ir) {
    if (ir.isRaw()) {
        if (ir.raw.empty()) return false;
        Serial.printf("[IR] Phát mã RAW, độ dài: %u, tần số: %ukHz\n", ir.raw.size(), ir.freq);
        irSend.sendRaw(ir.raw.data(), ir.raw.size(), ir.freq);
        return true;
    }

    uint64_t val = ir.getCodeUint64();
    String proto = ir.protocol;
    proto.toUpperCase();

    Serial.printf("[IR] Phát giao thức %s: 0x%llX (bits: %u)\n", proto.c_str(), val, ir.bits);

    if (proto == "NEC") {
        irSend.sendNEC(val, ir.bits);
    } else if (proto == "SAMSUNG") {
        irSend.sendSAMSUNG(val, ir.bits);
    } else if (proto == "SONY") {
        irSend.sendSony(val, ir.bits);
    } else if (proto == "LG") {
        irSend.sendLG(val, ir.bits);
    } else if (proto == "PANASONIC") {
        irSend.sendPanasonic64(val, ir.bits);
    } else if (proto == "RC5") {
        irSend.sendRC5(val, ir.bits);
    } else if (proto == "RC6") {
        irSend.sendRC6(val, ir.bits);
    } else {
        Serial.printf("[IR] Giao thức lạ %s, thử gửi NEC mặc định\n", proto.c_str());
        irSend.sendNEC(val, ir.bits);
    }
    return true;
}

bool IrSender::sendAcCommand(const String& protocolStr, const JsonObjectConst& params, int tempMin, int tempMax) {
    decode_type_t pType = strToDecodeType(protocolStr.c_str());
    if (pType == decode_type_t::UNKNOWN) {
        Serial.printf("[IR AC] Giao thức điều hòa không hỗ trợ: %s\n", protocolStr.c_str());
        return false;
    }

    // Đọc trạng thái từ params
    bool power = params["power"] | true;
    int temp = params["temp"] | 24;
    String mode = params["mode"] | "cool";
    String fan = params["fan"] | "auto";

    if (temp < tempMin) temp = tempMin;
    if (temp > tempMax) temp = tempMax;

    Serial.printf("[IR AC] Đặt điều hòa %s: Power=%d, Temp=%d, Mode=%s, Fan=%s\n", 
        protocolStr.c_str(), power, temp, mode.c_str(), fan.c_str());

    irAc.next.protocol = pType;
    irAc.next.power = power;
    irAc.next.degrees = temp;

    // Chế độ
    mode.toLowerCase();
    if (mode == "cool") irAc.next.mode = stdAc::opmode_t::kCool;
    else if (mode == "heat") irAc.next.mode = stdAc::opmode_t::kHeat;
    else if (mode == "dry") irAc.next.mode = stdAc::opmode_t::kDry;
    else if (mode == "fan") irAc.next.mode = stdAc::opmode_t::kFan;
    else irAc.next.mode = stdAc::opmode_t::kAuto;

    // Quạt
    fan.toLowerCase();
    if (fan == "min") irAc.next.fanspeed = stdAc::fanspeed_t::kMin;
    else if (fan == "low") irAc.next.fanspeed = stdAc::fanspeed_t::kLow;
    else if (fan == "medium") irAc.next.fanspeed = stdAc::fanspeed_t::kMedium;
    else if (fan == "high") irAc.next.fanspeed = stdAc::fanspeed_t::kHigh;
    else if (fan == "max") irAc.next.fanspeed = stdAc::fanspeed_t::kMax;
    else irAc.next.fanspeed = stdAc::fanspeed_t::kAuto;

    irAc.sendAc();
    return true;
}

