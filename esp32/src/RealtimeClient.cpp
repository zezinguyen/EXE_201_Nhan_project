#include "RealtimeClient.h"
#include "Provisioning.h"
#include "IrSender.h"
#include "IrLearner.h"
#include "DisplayOled.h"
#include "StatusLed.h"
#include "SupabaseRestClient.h"
#include <WiFi.h>
#include <ArduinoJson.h>

void RealtimeClient::begin() {
    wsClient.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
        this->onWebSocketEvent(type, payload, length);
    });
    wsClient.setReconnectInterval(2000);
}

void RealtimeClient::onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            connected = false;
            channelJoined = false;
            Serial.printf("[WSS] Mất kết nối Realtime WSS!\n");
            StatusLed::instance().setPattern(LED_WIFI_NO_INTERNET);
            DisplayOled::instance().showStatus("Mat Internet", "Dang ket noi lai...");
            // Tăng thời gian chờ kết nối lại 2s -> 4s -> ... -> 60s, ±20% ngẫu nhiên (Contract 3.1)
            reconnectInterval = min(reconnectInterval * 2, (unsigned long)60000);
            int jitter = (int)(reconnectInterval * 0.2f);
            long offset = (esp_random() % (2 * jitter + 1)) - jitter;
            unsigned long delayWithJitter = (unsigned long)max(1000L, (long)reconnectInterval + offset);
            wsClient.setReconnectInterval(delayWithJitter);
            break;

        case WStype_CONNECTED:
            connected = true;
            reconnectInterval = 2000;
            lastHeartbeatReply = millis();
            Serial.printf("[WSS] Đã kết nối WSS Supabase! Đang join channel...\n");
            joinBoardChannel();
            break;

        case WStype_TEXT: {
            String text = String((char*)payload);
            // Phoenix message: [join_ref, ref, topic, event, payload]
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, text);
            if (err) {
                return;
            }

            if (!doc.is<JsonArrayConst>()) return;
            JsonArrayConst arr = doc.as<JsonArrayConst>();
            if (arr.size() < 5) return;

            String topic = arr[2].as<String>();
            String event = arr[3].as<String>();
            JsonObjectConst pObj = arr[4].as<JsonObjectConst>();

            // Phản hồi heartbeat
            if (topic == "phoenix" && event == "phx_reply") {
                lastHeartbeatReply = millis();
                return;
            }

            // Phản hồi join channel
            if (event == "phx_reply" && topic.startsWith("realtime:board:")) {
                String status = pObj["status"] | "";
                if (status == "ok") {
                    channelJoined = true;
                    Serial.println("[WSS] Đã tham gia kênh riêng của mạch thành công!");
                    StatusLed::instance().setPattern(LED_ONLINE);
                    DisplayOled::instance().showStatus("Online", "San sang nhan lenh");
                    // Báo trạng thái ban đầu lên cloud
                    SupabaseRestClient::instance().sendStatusReport();
                } else {
                    Serial.printf("[WSS] Bị từ chối join channel: %s\n", status.c_str());
                }
                return;
            }

            // Xử lý Broadcast message từ Server
            if (event == "broadcast") {
                String bEvent = pObj["event"] | "";
                JsonObjectConst bPayload = pObj["payload"].as<JsonObjectConst>();
                handleBroadcastMessage(bEvent, bPayload);
            }
            break;
        }

        default:
            break;
    }
}

void RealtimeClient::joinBoardChannel() {
    String bId = Provisioning::instance().getBoardId();
    String token = Provisioning::instance().getBoardToken();
    String topic = "realtime:board:" + bId;

    // Join payload: {"config":{"broadcast":{"self":false},"private":true},"access_token":"<boardToken>"}
    JsonDocument joinPayloadDoc;
    JsonObject cfg = joinPayloadDoc["config"].to<JsonObject>();
    JsonObject bc = cfg["broadcast"].to<JsonObject>();
    bc["self"] = false;
    cfg["private"] = true;
    joinPayloadDoc["access_token"] = token;

    JsonDocument msgDoc;
    JsonArray arr = msgDoc.to<JsonArray>();
    arr.add("1");                 // join_ref
    arr.add("1");                 // ref
    arr.add(topic);               // topic
    arr.add("phx_join");          // event
    arr.add(joinPayloadDoc);      // payload

    String msg;
    serializeJson(msgDoc, msg);
    Serial.printf("[WSS] Gửi join: %s\n", msg.c_str());
    wsClient.sendTXT(msg);
}

void RealtimeClient::sendHeartbeat() {
    heartbeatSeq++;
    String refStr = "hb_" + String(heartbeatSeq);

    JsonDocument msgDoc;
    JsonArray arr = msgDoc.to<JsonArray>();
    arr.add((char*)nullptr);       // join_ref null
    arr.add(refStr);              // ref
    arr.add("phoenix");           // topic
    arr.add("heartbeat");         // event
    arr.add(JsonObjectConst());   // payload {}

    String msg;
    serializeJson(msgDoc, msg);
    wsClient.sendTXT(msg);
    lastHeartbeatSend = millis();
}

void RealtimeClient::handleBroadcastMessage(const String& event, const JsonObjectConst& payload) {
    Serial.printf("[WSS] Nhận broadcast: event=%s\n", event.c_str());

    if (event == "command") {
        String cmdId = payload["commandId"] | "";
        String devId = payload["deviceId"] | "";
        String action = payload["action"] | "";
        uint64_t expiresAt = payload["expiresAt"] | 0;
        uint32_t seq = payload["seq"] | 0;
        JsonObjectConst params = payload["params"].as<JsonObjectConst>();

        SendResult res = IrSender::instance().handleCommand(cmdId, devId, action, params, expiresAt, seq);
        if (res.error != "E_EXPIRED") {
            SupabaseRestClient::instance().sendAck(cmdId, res.ok, res.error, res.duplicate);
        }
    } 
    else if (event == "learn_start") {
        String reqId = payload["requestId"] | "";
        IrLearner::instance().startLearning(LEARN_FROM_APP, reqId);
    } 
    else if (event == "learn_cancel") {
        IrLearner::instance().cancelLearning();
    } 
    else if (event == "profile_updated") {
        int newVer = payload["profileVersion"] | 0;
        Serial.printf("[WSS] Server thông báo có profile mới v%d! (Đang cập nhật...)\n", newVer);
    }
}

void RealtimeClient::handleSerialBridge() {
    if (!SERIAL_BRIDGE_ENABLED || !Serial.available()) return;

    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.isEmpty() || !line.startsWith("{")) return;

    Serial.println("\n[SERIAL BRIDGE] Nhận JSON kiểm thử cục bộ:");
    Serial.println(line);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, line);
    if (!err) {
        String event = doc["event"] | "";
        JsonObjectConst payload = doc["payload"].as<JsonObjectConst>();
        handleBroadcastMessage(event, payload);
    }
}

void RealtimeClient::update() {
    handleSerialBridge();

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    String supaUrl = Provisioning::instance().getSupabaseUrl();
    String anonKey = Provisioning::instance().getSupabaseAnonKey();

    if (supaUrl.isEmpty() || anonKey.isEmpty()) {
        return; // Chưa cấu hình Supabase
    }

    // Tách host từ URL https://<project>.supabase.co
    String host = supaUrl;
    host.replace("https://", "");
    host.replace("http://", "");
    int slashIdx = host.indexOf('/');
    if (slashIdx != -1) host = host.substring(0, slashIdx);

    // Bắt đầu kết nối WSS nếu chưa kết nối
    static bool initiated = false;
    if (!initiated) {
        initiated = true;
        String path = "/realtime/v1/websocket?apikey=" + anonKey + "&vsn=1.0.0";
        Serial.printf("[WSS] Bắt đầu kết nối: wss://%s%s\n", host.c_str(), path.c_str());
        wsClient.beginSSL(host.c_str(), 443, path.c_str());
    }

    wsClient.loop();

    unsigned long now = millis();

    // Gửi Heartbeat mỗi 25 giây (Spec 5.1)
    if (connected && now - lastHeartbeatSend >= REALTIME_HEARTBEAT_MS) {
        sendHeartbeat();
    }

    // Kiểm tra timeout heartbeat 10 giây (Spec 5.1)
    if (connected && (now - lastHeartbeatSend > REALTIME_HEARTBEAT_TIMEOUT) && 
        (lastHeartbeatReply < lastHeartbeatSend)) {
        Serial.println("[WSS] Quá 10s Heartbeat không có trả lời -> Ngắt kết nối để nối lại");
        wsClient.disconnect();
    }
}

