#include "DeviceStore.h"

bool DeviceStore::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] Lỗi mount LittleFS!");
        return false;
    }
    Serial.println("[FS] Mount LittleFS thành công.");
    return loadProfile();
}

bool DeviceStore::loadProfile() {
    if (!LittleFS.exists(PROFILE_FILE_PATH)) {
        Serial.println("[FS] Chưa có file profile.json, tạo file mặc định...");
        File defaultFile = LittleFS.open(PROFILE_FILE_PATH, "w");
        if (defaultFile) {
            defaultFile.print(R"({
                "profileVersion": 1,
                "devices": {
                    "tv_living": {
                        "type": "tv",
                        "name": "Tivi phòng khách",
                        "commands": {
                            "POWER": {"protocol": "NEC", "code": "0x20DF10EF", "bits": 32},
                            "TV_POWER_ON": {"protocol": "NEC", "code": "0x20DF10EF", "bits": 32},
                            "TV_POWER_OFF": {"protocol": "NEC", "code": "0x20DF10EF", "bits": 32},
                            "VOL_UP": {"protocol": "NEC", "code": "0x20DF40BF", "bits": 32},
                            "VOL_DOWN": {"protocol": "NEC", "code": "0x20DFC03F", "bits": 32},
                            "MUTE": {"protocol": "NEC", "code": "0x20DF906F", "bits": 32},
                            "CH_VTV1": {"protocol": "NEC", "code": "0x20DF00FF", "bits": 32},
                            "CH_VTV3": {"protocol": "NEC", "code": "0x20DF08F7", "bits": 32},
                            "APP_YOUTUBE": {"protocol": "NEC", "code": "0x20DF58A7", "bits": 32}
                        }
                    },
                    "ac_living": {
                        "type": "ac",
                        "name": "Điều hòa phòng khách",
                        "irac_protocol": "PANASONIC_AC",
                        "limits": {"temp_min": 16, "temp_max": 30}
                    }
                }
            })");
            defaultFile.close();
        }
    }

    File file = LittleFS.open(PROFILE_FILE_PATH, "r");
    if (!file) {
        Serial.println("[FS] Không thể mở file profile.json");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        Serial.printf("[FS] Lỗi parse JSON profile: %s\n", err.c_str());
        return false;
    }

    profileVersion = doc["profileVersion"] | 1;
    devices.clear();

    JsonObjectConst devObj = doc["devices"].as<JsonObjectConst>();
    for (JsonPairConst kv : devObj) {
        String devId = kv.key().c_str();
        JsonObjectConst d = kv.value().as<JsonObjectConst>();

        DeviceEntry entry;
        entry.type = d["type"] | "tv";
        entry.name = d["name"] | devId;

        if (d["irac_protocol"].is<const char*>()) {
            entry.iracProtocol = d["irac_protocol"].as<String>();
            if (d["limits"].is<JsonObjectConst>()) {
                entry.tempMin = d["limits"]["temp_min"] | 16;
                entry.tempMax = d["limits"]["temp_max"] | 30;
            }
        }

        if (d["commands"].is<JsonObjectConst>()) {
            JsonObjectConst cmds = d["commands"].as<JsonObjectConst>();
            for (JsonPairConst ckv : cmds) {
                String action = ckv.key().c_str();
                IrCode ir = IrCode::fromJson(ckv.value().as<JsonObjectConst>());
                entry.commands[action] = ir;
            }
        }

        devices[devId] = entry;
    }

    Serial.printf("[FS] Đã tải profile v%d, số thiết bị: %u\n", profileVersion, devices.size());
    return true;
}

bool DeviceStore::saveProfile(const String& jsonString) {
    File file = LittleFS.open(PROFILE_FILE_PATH, "w");
    if (!file) {
        Serial.println("[FS] Lỗi mở file để ghi profile");
        return false;
    }
    file.print(jsonString);
    file.close();

    return loadProfile();
}

bool DeviceStore::hasDevice(const String& deviceId) {
    return devices.find(deviceId) != devices.end();
}

bool DeviceStore::findIrCode(const String& deviceId, const String& action, IrCode& outCode) {
    auto it = devices.find(deviceId);
    if (it == devices.end()) return false;

    auto cmdIt = it->second.commands.find(action);
    if (cmdIt == it->second.commands.end()) return false;

    outCode = cmdIt->second;
    return true;
}

bool DeviceStore::getAcConfig(const String& deviceId, String& outProtocol, int& outMin, int& outMax) {
    auto it = devices.find(deviceId);
    if (it == devices.end() || it->second.iracProtocol.isEmpty()) return false;

    outProtocol = it->second.iracProtocol;
    outMin = it->second.tempMin;
    outMax = it->second.tempMax;
    return true;
}

