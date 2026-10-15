#include "modules/WifiManager.h"

#include <Arduino.h>
#include <Password.h>
#include <WiFi.h>

#include "config.h"
#include "modules/Board.h"

namespace {
volatile uint8_t lastDisconnectReason = 0;  // Ghi từ tác vụ sự kiện WiFi
}  // namespace

WifiManager::WifiManager() : backoff_(RECONNECT_BASE_MS, RECONNECT_MAX_MS, RECONNECT_JITTER_PERCENT) {}

void WifiManager::begin(SecureStore& store) {
    store_ = &store;
    WiFi.persistent(false);  // Tự quản lý danh sách mạng trong SecureStore
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    // Nhiều điểm phát cùng tên (router + kích sóng): quét hết kênh, chọn điểm sóng mạnh nhất
    // thay vì điểm đầu tiên tìm thấy
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
    // Lý do rớt/không vào được WiFi (sai mật khẩu, không thấy mạng...) – không log SSID
    WiFi.onEvent(
        [](WiFiEvent_t, WiFiEventInfo_t info) {
            const auto reason = static_cast<wifi_err_reason_t>(info.wifi_sta_disconnected.reason);
            lastDisconnectReason = reason;
            log_w("WiFi ngat, ly do %u (%s)", reason, WiFi.disconnectReasonName(reason));
        },
        ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    disconnectedSinceMs_ = millis();
    nextAttemptMs_ = disconnectedSinceMs_;
}

void WifiManager::update(uint32_t nowMs) {
    updatePortal(nowMs);
    if (isConnected()) {
        if (connecting_) {
            connecting_ = false;
            backoff_.reset();
            connectedAtMs_ = nowMs;
            log_i("WiFi da ket noi, IP %s", WiFi.localIP().toString().c_str());
            // Chỉ báo khi chính lần thử này thành công (ngay sau disconnect() status có thể vẫn là mạng cũ)
            if (portalAttempt_) {
                portalAttempt_ = false;
                portal_.setStationState(PortalStationState::Connected);
            }
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
        log_w("Het %lus chua vao duoc WiFi #%u (status %d)", WIFI_CONNECT_TIMEOUT_MS / 1000, networkIndex_,
              WiFi.status());
        WiFi.disconnect();
        if (portalAttempt_) {
            portalAttempt_ = false;
            portal_.setStationState(PortalStationState::Failed, lastDisconnectReason);
        }
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

void WifiManager::localIp(char* out, size_t outSize) const {
    if (outSize == 0) {
        return;
    }
    out[0] = '\0';
    if (isConnected()) {
        snprintf(out, outSize, "%s", WiFi.localIP().toString().c_str());
    }
}

void WifiManager::updatePortal(uint32_t nowMs) {
    if (!portalActive_) {
        return;
    }
    WifiCredential credential;
    if (portal_.update(credential)) {
        store_->addWifi(credential);  // Lên đầu danh sách -> thử ngay
        memset(&credential, 0, sizeof(credential));
        log_i("Nhan WiFi moi tu trang cai dat");
        WiFi.disconnect();
        connecting_ = false;
        networkIndex_ = 0;
        backoff_.reset();
        nextAttemptMs_ = nowMs;
        portalSubmitted_ = true;
        portalAttempt_ = true;
        return;
    }
    if (!isConnected() || portalAttempt_) {
        return;
    }
    const bool lingerDone = portalSubmitted_ && nowMs - connectedAtMs_ >= WIFI_SETUP_LINGER_MS;
    const bool idleDone = !portalSubmitted_ && nowMs - portalStartMs_ >= WIFI_SETUP_IDLE_MS;
    if (lingerDone || idleDone) {
        log_i("Tat WiFi cai dat");
        stopSetupPortal();
    }
}

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
    portalSubmitted_ = false;
    portalAttempt_ = false;
    portalStartMs_ = nowMs;
    portal_.begin();
    log_i("Bat WiFi cai dat: %s", setupSsid_);  // Không log mật khẩu
}

void WifiManager::stopSetupPortal() {
    if (!portalActive_) {
        return;
    }
    portal_.end();
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
    lastDisconnectReason = 0;
    WiFi.begin(credential.ssid, credential.password);
    connecting_ = true;
    attemptStartMs_ = nowMs;
}
