#pragma once
#include <Arduino.h>
#include <WebSocketsClient.h>
#include "config.h"

class RealtimeClient {
public:
    static RealtimeClient& instance() {
        static RealtimeClient client;
        return client;
    }

    void begin();
    void update();
    bool isConnected() const { return connected; }

private:
    RealtimeClient() = default;

    WebSocketsClient wsClient;
    bool connected = false;
    bool channelJoined = false;

    unsigned long lastHeartbeatSend = 0;
    unsigned long lastHeartbeatReply = 0;
    uint32_t heartbeatSeq = 0;

    unsigned long reconnectInterval = 2000;
    unsigned long lastDisconnectTime = 0;

    void onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);
    void joinBoardChannel();
    void sendHeartbeat();
    void handleBroadcastMessage(const String& event, const JsonObjectConst& payload);
    void handleSerialBridge();
};

