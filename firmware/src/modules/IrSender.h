#pragma once
#include <ArduinoJson.h>
#include <stdint.h>

struct AcRequest {
    const char* protocol;  // Tên protocol IRac, VD "PANASONIC_AC"
    bool power;
    float temperatureC;
    const char* mode;      // "cool" | "heat" | "dry" | "fan" | "auto"
    const char* fan;       // "auto" | "low" | "medium" | "high"
};

// Phát IR: mã theo protocol, raw, hoặc trạng thái điều hòa (IRac).
class IrSender {
public:
    void begin();
    // protocol: tên theo IRremoteESP8266, VD "NEC", "SAMSUNG"
    bool sendCode(const char* protocol, uint64_t value, uint16_t bits);
    bool sendRaw(const uint16_t* durations, uint16_t length, uint16_t khz);
    bool sendAc(const AcRequest& request);

    // Giao thức điều hòa gửi được qua IRac (VD "MIDEA")
    static bool isAcProtocolSupported(const char* protocol);
    // Thêm tên mọi giao thức điều hòa IRac hỗ trợ vào out
    static void listAcProtocols(JsonArray out);
};
