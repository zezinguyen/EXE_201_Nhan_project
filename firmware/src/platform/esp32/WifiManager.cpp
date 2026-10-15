#include "modules/WifiManager.h"

#include <Arduino.h>
#include <Password.h>
#include <WiFi.h>

#include "config.h"
#include "modules/Board.h"

WifiManager::WifiManager() : backoff_(RECONNECT_BASE_MS, RECONNECT_MAX_MS, RECONNECT_JITTER_PERCENT) {}

void WifiManager::begin(SecureStore& store) {
    store_ = &store;
    WiFi.persistent(false);  // Tự quản lý danh sách mạng trong SecureStore
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    disconnectedSinceMs_ = millis();
    nextAttemptMs_ = disconnectedSinceMs_;
}

void WifiManager::update(uint32_t nowMs) {
    if (isConnected()) {
        if (connecting_) {
            connecting_ = false;
            backoff_.reset();
            log_i("WiFi da ket noi, IP %s", WiFi.localIP().toString().c_str());
        }
        disconnectedSinceMs_ = nowMs;
        return;
    }

    if (!portalActive_ && nowMs - disconnectedSinceMs_ >= WIFI_SETUP_AP_AFTER_MS) {
        startSetupPortal(nowMs);
    }

    if (connecting_) {
        if (nowMs - attemptStartMs_ < WIFI_CONNECT_TIMEOUT_MS) {
            return;
        }
        connecting_ = false;
        WiFi.disconnect();
        networkIndex_++;
        // Đã thử hết danh sách -> chờ theo backoff rồi thử lại từ đầu
        if (networkIndex_ >= store_->wifiCount()) {
            networkIndex_ = 0;
            nextAttemptMs_ = nowMs + backoff_.nextDelayMs(boardRandom());
        } else {
            nextAttemptMs_ = nowMs;
        }
    }

    if (static_cast<int32_t>(nowMs - nextAttemptMs_) >= 0) {
        tryNextNetwork(nowMs);
    }
}

bool WifiManager::isConnected() const { return WiFi.status() == WL_CONNECTED; }

int8_t WifiManager::rssi() const { return isConnected() ? WiFi.RSSI() : 0; }

void WifiManager::startSetupPortal(uint32_t nowMs) {
    if (portalActive_) {
        return;
    }
    snprintf(setupSsid_, sizeof(setupSsid_), "%s%04X", WIFI_SETUP_AP_PREFIX,
             static_cast<uint16_t>(ESP.getEfuseMac() & 0xFFFF));
    // Mật khẩu mới mỗi lần bật, chỉ hiện trên OLED: phải đứng trước mạch mới nối được
    if (!generatePassword(setupPassword_, sizeof(setupPassword_), WIFI_SETUP_AP_PASSWORD_LENGTH, boardRandom)) {
        return;
    }
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(setupSsid_, setupPassword_);
    portalActive_ = true;
    log_i("Bat WiFi cai dat: %s", setupSsid_);  // Không log mật khẩu
    // TODO(tuần 5): WebServer trang nhập WiFi -> store_->addWifi(), rồi stopSetupPortal()
    (void)nowMs;
}

void WifiManager::stopSetupPortal() {
    if (!portalActive_) {
        return;
    }
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    portalActive_ = false;
    memset(setupPassword_, 0, sizeof(setupPassword_));
}

void WifiManager::tryNextNetwork(uint32_t nowMs) {
    WifiCredential credential;
    if (store_ == nullptr || !store_->loadWifi(networkIndex_, credential)) {
        // Chưa có mạng nào: chờ người dùng cài qua portal
        nextAttemptMs_ = nowMs + RECONNECT_MAX_MS;
        return;
    }
    log_i("Thu ket noi WiFi #%u", networkIndex_);  // Không log SSID/mật khẩu
    WiFi.begin(credential.ssid, credential.password);
    connecting_ = true;
    attemptStartMs_ = nowMs;
}
