#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <map>
#include <ArduinoJson.h>
#include "config.h"
#include "IrCode.h"

struct DeviceEntry {
    String type;
    String name;
    String iracProtocol; // Nếu là điều hòa
    int tempMin = 16;
    int tempMax = 30;
    std::map<String, IrCode> commands; // action -> IrCode (nếu là TV hoặc thiết bị phím bấm)
};

class DeviceStore {
public:
    static DeviceStore& instance() {
        static DeviceStore store;
        return store;
    }

    bool begin();
    bool loadProfile();
    bool saveProfile(const String& jsonString);

    int getProfileVersion() const { return profileVersion; }
    bool findIrCode(const String& deviceId, const String& action, IrCode& outCode);
    bool getAcConfig(const String& deviceId, String& outProtocol, int& outMin, int& outMax);
    bool hasDevice(const String& deviceId);

private:
    DeviceStore() = default;

    int profileVersion = 1;
    std::map<String, DeviceEntry> devices;
};

