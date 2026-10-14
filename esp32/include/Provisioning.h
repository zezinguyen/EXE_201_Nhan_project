#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include "config.h"

class Provisioning {
public:
    static Provisioning& instance() {
        static Provisioning prov;
        return prov;
    }

    void begin();
    void update();

    bool isPaired() const { return paired; }
    String getBoardId() const { return boardId; }
    String getBoardToken() const { return boardToken; }
    String getHomeId() const { return homeId; }
    String getWifiSsid() const { return wifiSsid; }
    String getWifiPassword() const { return wifiPassword; }
    String getSupabaseUrl() const { return supabaseUrl; }
    String getSupabaseAnonKey() const { return supabaseAnonKey; }

    void startPairingMode();
    void stopPairingMode();
    bool isInPairingMode() const { return inPairingMode; }

    void saveCredentials(const String& bId, const String& bToken, const String& hId);
    void saveWifi(const String& ssid, const String& pass);
    void saveSupabaseConfig(const String& url, const String& anon);
    void factoryReset();

private:
    Provisioning() = default;

    Preferences prefs;
    WebServer server{80};

    bool paired = false;
    bool inPairingMode = false;
    unsigned long pairingStartTime = 0;

    String boardId = "board_living_01";
    String boardToken = "";
    String homeId = "00000000-0000-0000-0000-000000000001";
    String wifiSsid = "";
    String wifiPassword = "";
    String supabaseUrl = "";
    String supabaseAnonKey = "";

    String apSsid = "";
    String apPassword = "";
    String pairingCode = "";

    String generateRandomString(size_t len);
    String generateSixDigitCode();
    void setupApWebServer();
};

