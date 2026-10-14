#pragma once
#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>

struct IrCode {
    String protocol = "UNKNOWN";
    String code = "0x00000000";
    uint16_t bits = 32;
    std::vector<uint16_t> raw;
    uint16_t freq = 38;

    bool isRaw() const {
        return !raw.empty() || protocol.equalsIgnoreCase("UNKNOWN");
    }

    uint64_t getCodeUint64() const {
        String hexStr = code;
        hexStr.trim();
        if (hexStr.startsWith("0x") || hexStr.startsWith("0X")) {
            hexStr = hexStr.substring(2);
        }
        uint64_t result = 0;
        for (size_t i = 0; i < hexStr.length(); i++) {
            char c = hexStr.charAt(i);
            int val = 0;
            if (c >= '0' && c <= '9') val = c - '0';
            else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
            else break;
            result = (result << 4) | val;
        }
        return result;
    }

    void toJson(JsonObject& obj) const {
        if (!isRaw() && !protocol.isEmpty()) {
            obj["protocol"] = protocol;
            obj["code"] = code;
            obj["bits"] = bits;
        } else {
            obj["protocol"] = "UNKNOWN";
            JsonArray rawArr = obj["raw"].to<JsonArray>();
            for (uint16_t val : raw) {
                rawArr.add(val);
            }
            obj["freq"] = freq;
        }
    }

    static IrCode fromJson(const JsonObjectConst& obj) {
        IrCode result;
        if (obj["raw"].is<JsonArrayConst>()) {
            result.protocol = "UNKNOWN";
            JsonArrayConst arr = obj["raw"].as<JsonArrayConst>();
            for (JsonVariantConst v : arr) {
                result.raw.push_back(v.as<uint16_t>());
            }
            result.freq = obj["freq"] | 38;
        } else {
            result.protocol = obj["protocol"] | "NEC";
            result.code = obj["code"] | "0x00000000";
            result.bits = obj["bits"] | 32;
        }
        return result;
    }
};

