#pragma once
#include <ArduinoJson.h>
#include <Backoff.h>
#include <WebSocketsClient.h>
#include <stdint.h>

using BroadcastHandler = void (*)(const char* event, JsonObjectConst payload);

// Kết nối WSS tới Supabase Realtime (giao thức Phoenix): vào kênh riêng của nhà,
// heartbeat, nhận broadcast (lệnh), gửi broadcast (ACK), Presence (spec 4, 5.1).
class RealtimeClient {
public:
    RealtimeClient();

    // channelName: VD "home:<homeId>" (topic gửi đi là "realtime:" + channelName)
    void begin(const char* accessToken, const char* channelName, const char* presenceKey,
               BroadcastHandler handler);
    void end();
    void update(uint32_t nowMs);

    bool isStarted() const { return started_; }
    bool isJoined() const { return joined_; }

    bool broadcast(const char* event, const JsonDocument& payload);
    bool trackPresence(const JsonDocument& state);

private:
    void onSocketEvent(WStype_t type, uint8_t* payload, size_t length);
    void onMessage(const char* text, size_t length);
    void sendJoin();
    void sendHeartbeat(uint32_t nowMs);
    bool send(const char* topic, const char* event, const JsonDocument& payload, uint32_t* refOut = nullptr);

    WebSocketsClient socket_;
    Backoff backoff_;
    BroadcastHandler handler_ = nullptr;
    String accessToken_;
    String topic_;
    String presenceKey_;
    bool started_ = false;
    bool joined_ = false;
    uint32_t nextRef_ = 1;
    uint32_t joinRef_ = 0;
    uint32_t heartbeatRef_ = 0;
    uint32_t lastHeartbeatMs_ = 0;
    bool heartbeatPending_ = false;
};
