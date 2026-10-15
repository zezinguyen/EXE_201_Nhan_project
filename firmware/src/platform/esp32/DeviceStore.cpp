#include "modules/DeviceStore.h"

#include <Arduino.h>
#include <LittleFS.h>

#include "config.h"

namespace {
bool readFile(const char* path, String& out) {
    File file = LittleFS.open(path, "r");
    if (!file) {
        return false;
    }
    out = file.readString();
    file.close();
    return out.length() > 0;
}
}  // namespace

bool DeviceStore::begin() {
    // formatOnFail = true: lần đầu chưa có filesystem thì tự format
    return LittleFS.begin(true);
}

bool DeviceStore::load() {
    String json;
    if (readFile(PROFILE_PATH, json) && loadFromJson(json.c_str(), json.length())) {
        return true;
    }
    log_w("Profile hong/khong co, thu ban sao luu");
    return readFile(PROFILE_BACKUP_PATH, json) && loadFromJson(json.c_str(), json.length());
}

bool DeviceStore::save(const char* json, size_t length) {
    if (!loadFromJson(json, length)) {
        return false;  // E_PROFILE_INVALID: giữ nguyên profile cũ
    }

    File file = LittleFS.open(PROFILE_TEMP_PATH, "w");
    if (!file) {
        return false;
    }
    const size_t written = file.write(reinterpret_cast<const uint8_t*>(json), length);
    file.close();
    if (written != length) {
        LittleFS.remove(PROFILE_TEMP_PATH);
        return false;
    }

    if (LittleFS.exists(PROFILE_BACKUP_PATH)) {
        LittleFS.remove(PROFILE_BACKUP_PATH);
    }
    if (LittleFS.exists(PROFILE_PATH)) {
        LittleFS.rename(PROFILE_PATH, PROFILE_BACKUP_PATH);
    }
    return LittleFS.rename(PROFILE_TEMP_PATH, PROFILE_PATH);
}
