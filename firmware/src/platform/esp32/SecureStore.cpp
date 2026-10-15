#include "modules/SecureStore.h"

#include <Preferences.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr const char* kNamespace = "secure";
Preferences prefs;

void wifiKey(char* out, size_t size, uint8_t index, const char* field) {
    snprintf(out, size, "wifi%u_%s", index, field);
}
}  // namespace

bool SecureStore::begin() { return prefs.begin(kNamespace, false); }

uint8_t SecureStore::wifiCount() const { return prefs.getUChar("wifi_count", 0); }

bool SecureStore::loadWifi(uint8_t index, WifiCredential& out) const {
    if (index >= wifiCount()) {
        return false;
    }
    char key[24];
    wifiKey(key, sizeof(key), index, "ssid");
    if (prefs.getString(key, out.ssid, sizeof(out.ssid)) == 0) {
        return false;
    }
    wifiKey(key, sizeof(key), index, "pass");
    if (prefs.getString(key, out.password, sizeof(out.password)) == 0) {
        out.password[0] = '\0';  // Mạng mở
    }
    return true;
}

bool SecureStore::addWifi(const WifiCredential& credential) {
    WifiCredential list[WIFI_MAX_NETWORKS];
    uint8_t count = 0;
    list[count++] = credential;
    for (uint8_t i = 0; i < wifiCount() && count < WIFI_MAX_NETWORKS; i++) {
        WifiCredential old;
        if (loadWifi(i, old) && strcmp(old.ssid, credential.ssid) != 0) {
            list[count++] = old;
        }
    }

    char key[24];
    for (uint8_t i = 0; i < count; i++) {
        wifiKey(key, sizeof(key), i, "ssid");
        prefs.putString(key, list[i].ssid);
        wifiKey(key, sizeof(key), i, "pass");
        prefs.putString(key, list[i].password);
    }
    return prefs.putUChar("wifi_count", count) > 0;
}

bool SecureStore::hasBoardToken() const { return prefs.isKey("token"); }

bool SecureStore::loadBoardToken(char* out, size_t outSize) const {
    return prefs.getString("token", out, outSize) > 0;
}

bool SecureStore::saveBoardToken(const char* token) { return prefs.putString("token", token) > 0; }

bool SecureStore::loadBoardId(char* out, size_t outSize) const {
    return prefs.getString("board_id", out, outSize) > 0;
}

bool SecureStore::saveBoardId(const char* boardId) { return prefs.putString("board_id", boardId) > 0; }

bool SecureStore::loadHomeId(char* out, size_t outSize) const {
    return prefs.getString("home_id", out, outSize) > 0;
}

bool SecureStore::saveHomeId(const char* homeId) { return prefs.putString("home_id", homeId) > 0; }

void SecureStore::clearAll() { prefs.clear(); }
