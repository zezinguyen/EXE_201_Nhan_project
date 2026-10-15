#include "ProfileEdit.h"

#include <string.h>

namespace {
void bumpVersion(JsonDocument& doc) { doc["profileVersion"] = (doc["profileVersion"] | 0u) + 1; }

JsonObject devicesOf(JsonDocument& doc) {
    if (!doc["profileVersion"].is<uint32_t>()) {
        doc["profileVersion"] = 0;
    }
    JsonObject devices = doc["devices"];
    return devices.isNull() ? doc["devices"].to<JsonObject>() : devices;
}

bool isAc(JsonObjectConst device) { return device["irac_protocol"].is<const char*>(); }

bool isValidIrCode(JsonObjectConst ir) {
    if (ir["raw"].is<JsonArrayConst>()) {
        return ir["raw"].size() > 0;
    }
    return ir["protocol"].is<const char*>() && ir["code"].is<const char*>() && ir["bits"].is<uint16_t>();
}
}  // namespace

bool isValidProfileName(const char* name, size_t maxBytes) {
    if (name == nullptr || name[0] == '\0') {
        return false;
    }
    size_t length = 0;
    for (const char* p = name; *p != '\0'; p++, length++) {
        if (length >= maxBytes || static_cast<unsigned char>(*p) < 0x20 || *p == 0x7F) {
            return false;
        }
    }
    return true;
}

bool profileEnsureAc(JsonDocument& doc, const char* deviceId, const char* name, const char* protocol) {
    if (deviceId == nullptr || protocol == nullptr) {
        return false;
    }
    JsonObject devices = devicesOf(doc);
    if (!devices[deviceId].isNull()) {
        return false;
    }
    JsonObject device = devices[deviceId].to<JsonObject>();
    device["type"] = "ac";
    device["name"] = name;
    device["irac_protocol"] = protocol;
    bumpVersion(doc);
    return true;
}

bool profileSetAcProtocol(JsonDocument& doc, const char* deviceId, const char* protocol) {
    if (deviceId == nullptr || protocol == nullptr) {
        return false;
    }
    JsonObject device = doc["devices"][deviceId];
    if (device.isNull() || !isAc(device)) {
        return false;
    }
    if (strcmp(device["irac_protocol"], protocol) == 0) {
        return false;
    }
    device["irac_protocol"] = protocol;
    bumpVersion(doc);
    return true;
}

bool profilePutCommand(JsonDocument& doc, const char* deviceId, const char* deviceName, const char* action,
                       JsonObjectConst ir) {
    if (deviceId == nullptr || action == nullptr || !isValidIrCode(ir)) {
        return false;
    }
    JsonObject devices = devicesOf(doc);
    JsonObject device = devices[deviceId];
    if (device.isNull()) {
        device = devices[deviceId].to<JsonObject>();
        device["type"] = "custom";
        device["name"] = deviceName != nullptr ? deviceName : deviceId;
    } else if (isAc(device)) {
        return false;
    }
    JsonObject commands = device["commands"];
    if (commands.isNull()) {
        commands = device["commands"].to<JsonObject>();
    }
    commands[action].set(ir);
    bumpVersion(doc);
    return true;
}

bool profileRemoveCommand(JsonDocument& doc, const char* deviceId, const char* action) {
    if (deviceId == nullptr || action == nullptr) {
        return false;
    }
    JsonObject commands = doc["devices"][deviceId]["commands"];
    if (commands.isNull() || commands[action].isNull()) {
        return false;
    }
    commands.remove(action);
    bumpVersion(doc);
    return true;
}
