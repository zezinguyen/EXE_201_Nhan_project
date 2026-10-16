#include "DeviceStore.h"

#include <Arduino.h>

#include "config.h"

uint32_t DeviceStore::profileVersion() const { return doc_["profileVersion"] | 0u; }

JsonObjectConst DeviceStore::findDevice(const char* deviceId) const {
    if (deviceId == nullptr) {
        return JsonObjectConst();
    }
    return doc_["devices"][deviceId].as<JsonObjectConst>();
}

JsonObjectConst DeviceStore::findCommand(const char* deviceId, const char* action) const {
    if (action == nullptr) {
        return JsonObjectConst();
    }
    return findDevice(deviceId)["commands"][action].as<JsonObjectConst>();
}

bool DeviceStore::isValidProfile(const JsonDocument& doc) {
    return doc["profileVersion"].is<uint32_t>() && doc["devices"].is<JsonObjectConst>();
}

bool DeviceStore::loadFromJson(const char* json, size_t length) {
    JsonDocument incoming;
    if (deserializeJson(incoming, json, length) != DeserializationError::Ok || !isValidProfile(incoming)) {
        return false;
    }
    doc_ = std::move(incoming);
    return true;
}

bool DeviceStore::save(const JsonDocument& doc) {
    const size_t length = measureJson(doc);
    if (length > PROFILE_MAX_BYTES) {
        return false;
    }
    String json;
    json.reserve(length + 1);
    serializeJson(doc, json);
    return save(json.c_str(), json.length());
}
