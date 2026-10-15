#include "modules/SupabaseRest.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <string.h>

#include "config.h"

void SupabaseRest::begin(const char* boardToken, const char* homeId, const char* boardId) {
    strncpy(token_, boardToken, sizeof(token_) - 1);
    strncpy(homeId_, homeId, sizeof(homeId_) - 1);
    strncpy(boardId_, boardId, sizeof(boardId_) - 1);
    ready_ = true;
}

void SupabaseRest::end() {
    memset(token_, 0, sizeof(token_));
    ready_ = false;
}

bool SupabaseRest::insertRow(const char* table, const JsonDocument& row) {
    if (!ready_) {
        return false;
    }
    WiFiClientSecure client;
    client.setCACert(SUPABASE_ROOT_CA);  // Không setInsecure()

    HTTPClient http;
    const String url = String("https://") + SUPABASE_HOST + "/rest/v1/" + table;
    if (!http.begin(client, url)) {
        return false;
    }
    http.setTimeout(REST_TIMEOUT_MS);
    http.addHeader("apikey", SUPABASE_ANON_KEY);
    http.addHeader("Authorization", String("Bearer ") + token_);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Prefer", "return=minimal");

    String body;
    serializeJson(row, body);
    const int status = http.POST(body);
    http.end();
    if (status != 201) {
        log_w("Ghi bang %s that bai: HTTP %d", table, status);
    }
    return status == 201;
}
