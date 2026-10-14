/*
 * =========================================================================
 * ĐỒ ÁN NHÀ THÔNG MINH - HỆ THỐNG MẠCH IR ĐIỀU KHIỂN THIẾT BỊ BẰNG GIỌNG NÓI
 * Phiên bản: v0.1.0 (Tuân thủ specs.md, contracts.md, production-notes.md v3)
 * Vi điều khiển: ESP32 (Chạy pin LiPo 3.7V, Modem-Sleep, CPU 80MHz)
 * Kết nối: Supabase Realtime WSS (kênh riêng board:<boardId>), HTTPS REST/RPC
 * Phần cứng:
 *   - Mắt phát IR (GPIO4) qua transistor S8050
 *   - Mắt thu IR VS1838B (OUT GPIO18, VCC GPIO19 qua cổng nguồn GPIO)
 *   - DFPlayer Mini (UART2: RX2 GPIO16, TX2 GPIO17)
 *   - Màn hình OLED SSD1306 0.96" I2C (SDA GPIO21, SCL GPIO22)
 *   - LED RGB Anode chung (R GPIO25, G GPIO26, B GPIO27)
 *   - Nút nhấn đa năng 1 nút (GPIO13 to GND)
 *   - Cầu chia đo Pin B+ (VP/GPIO36), đo nguồn USB (VN/GPIO39)
 * =========================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRac.h>
#include <IRutils.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include <DFRobotDFPlayerMini.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <time.h>
#include <esp_wifi.h>
#include <vector>
#include <map>

// ================= CẤU HÌNH NGOẠI VI & CHÂN GPIO =================
#define PIN_IR_SEND        4
#define PIN_IR_RECV_OUT   18
#define PIN_IR_RECV_VCC   19
#define PIN_DFPLAYER_RX   16
#define PIN_DFPLAYER_TX   17
#define PIN_OLED_SDA      21
#define PIN_OLED_SCL      22
#define OLED_I2C_ADDR   0x3C
#define PIN_RGB_R         25
#define PIN_RGB_G         26
#define PIN_RGB_B         27
#define PIN_BUTTON        13
#define PIN_BATTERY_ADC   36
#define PIN_USB_ADC       39

#define CPU_FREQ_MHZ              80
#define REALTIME_HEARTBEAT_MS     25000
#define STATUS_REPORT_INTERVAL_MS 300000
#define OLED_SLEEP_TIMEOUT_MS     10000
#define LEARN_TIMEOUT_MS          15000
#define FIRMWARE_VERSION          "0.1.0"

// Khởi tạo các đối tượng điều khiển ngoại vi
IRsend irsend(PIN_IR_SEND);
IRac irac(PIN_IR_SEND);
IRrecv irrecv(PIN_IR_RECV_OUT, 1024, 50, true);
decode_results irResults;

SSD1306Wire oled(OLED_I2C_ADDR, PIN_OLED_SDA, PIN_OLED_SCL);
DFRobotDFPlayerMini dfPlayer;
HardwareSerial dfSerial(2);
WebSocketsClient wsClient;
Preferences prefs;

// ================= TRẠNG THÁI HỆ THỐNG =================
String boardId = "board_living_01";
String homeId = "00000000-0000-0000-0000-000000000001";
String boardToken = "";
String wifiSsid = "";
String wifiPass = "";
String supabaseUrl = "";
String supabaseAnonKey = "";

int batteryPercent = 100;
float batteryVoltage = 4.0f;
bool isCharging = false;
bool isUsb = false;
bool screenPowered = true;
unsigned long lastScreenWake = 0;
bool oledLockAwake = false;

// Đèn LED trạng thái
enum LedState { LED_ST_ONLINE, LED_ST_WIFI_NO_INTERNET, LED_ST_NO_WIFI, LED_ST_SETUP, LED_ST_LEARN };
LedState ledState = LED_ST_NO_WIFI;
unsigned long lastLedToggle = 0;
int ledStep = 0;

// Bộ nhớ đệm 16 commandId gần nhất chống lệnh lặp (Contract 3.2)
std::vector<String> recentCmdIds;
uint32_t cmdTimestamps[5] = {0};
uint8_t rateIdx = 0;

// Chế độ học lệnh
bool isLearning = false;
int learnStep = 1;
unsigned long learnStartTime = 0;
String currentLearnRequestId = "";
bool learnFromApp = false;
String firstCaptureProto = "";
String firstCaptureCode = "";
uint16_t firstCaptureBits = 32;

// Heartbeat Realtime
unsigned long lastHbSend = 0;
unsigned long lastHbReply = 0;
uint32_t hbSeq = 0;
bool wsConnected = false;
bool wsChannelJoined = false;

// ================= HÀM ĐIỀU KHIỂN NGOẠI VI =================
void setRgb(bool r, bool g, bool b) {
  digitalWrite(PIN_RGB_R, r ? LOW : HIGH);
  digitalWrite(PIN_RGB_G, g ? LOW : HIGH);
  digitalWrite(PIN_RGB_B, b ? LOW : HIGH);
}

void playAudioTrack(uint16_t track) {
  dfPlayer.playMp3Folder(track);
}

void wakeScreen() {
  screenPowered = true;
  lastScreenWake = millis();
  oled.displayOn();
}

void readBattery() {
  uint32_t rawBat = 0, rawUsb = 0;
  for (int i = 0; i < 8; i++) {
    rawBat += analogRead(PIN_BATTERY_ADC);
    rawUsb += analogRead(PIN_USB_ADC);
    delayMicroseconds(100);
  }
  rawBat /= 8; rawUsb /= 8;
  const float calib = 1.05f;
  batteryVoltage = (rawBat / 4095.0f) * 3.3f * 2.0f * calib;
  float vUsb = (rawUsb / 4095.0f) * 3.3f * 2.0f * calib;
  isUsb = (vUsb >= 4.2f);
  isCharging = (isUsb && batteryVoltage < 4.15f);

  if (batteryVoltage <= 3.3f) batteryPercent = 0;
  else if (batteryVoltage >= 4.2f) batteryPercent = 100;
  else batteryPercent = (int)((batteryVoltage - 3.3f) / (4.2f - 3.3f) * 100.0f);
}

void drawOled(const String& mainText, const String& subText = "") {
  if (!screenPowered) return;
  oled.clear();
  
  // Header: Pin, USB, RSSI
  String batStr = String(batteryPercent) + "%" + (isCharging ? "+" : (isUsb ? "=" : ""));
  oled.drawString(0, 0, batStr);
  String wifiStr = (WiFi.status() == WL_CONNECTED) ? (String(WiFi.RSSI()) + "dBm") : "No WiFi";
  oled.drawString(128 - oled.getStringWidth(wifiStr), 0, wifiStr);
  oled.drawHorizontalLine(0, 13, 128);

  // Body
  oled.setFont(ArialMT_Plain_16);
  oled.drawString(0, 20, mainText);
  oled.setFont(ArialMT_Plain_10);
  if (!subText.isEmpty()) oled.drawString(0, 44, subText);
  oled.display();
}

// ================= GỌI SUPABASE HTTPS REST/RPC =================
bool callRpc(const String& rpcName, const String& jsonBody) {
  if (WiFi.status() != WL_CONNECTED || supabaseUrl.isEmpty() || supabaseAnonKey.isEmpty()) return false;
  String url = supabaseUrl + "/rest/v1/rpc/" + rpcName;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", supabaseAnonKey);
  http.addHeader("Authorization", "Bearer " + (boardToken.isEmpty() ? supabaseAnonKey : boardToken));
  http.setTimeout(4000);
  int code = http.POST(jsonBody);
  http.end();
  return (code >= 200 && code < 300);
}

void sendAck(const String& cmdId, bool ok, const String& error = "", bool dup = false) {
  JsonDocument doc;
  doc["commandId"] = cmdId;
  doc["ok"] = ok;
  if (!ok && !error.isEmpty()) doc["error"] = error;
  if (dup) doc["duplicate"] = true;
  String body;
  serializeJson(doc, body);
  callRpc("board_ack", body);
}

void sendBoardStatus() {
  JsonDocument doc;
  doc["battery"] = batteryPercent;
  doc["charging"] = isCharging;
  doc["usb"] = isUsb;
  doc["rssi"] = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -100;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["profileVersion"] = 1;
  String body;
  serializeJson(doc, body);
  callRpc("board_status", body);
}

// ================= PHÁT LỆNH HỒNG NGOẠI =================
bool sendIrCommand(const String& cmdId, const String& deviceId, const String& action, const JsonObjectConst& params) {
  // 1. Chống gửi lặp (Contract 3.2)
  for (const String& id : recentCmdIds) {
    if (id == cmdId) {
      sendAck(cmdId, true, "", true);
      return true;
    }
  }

  // 2. Nếu đang học lệnh -> Từ chối phát IR (E_BUSY, Contract 3.4 & 3.5)
  if (isLearning) {
    sendAck(cmdId, false, "E_BUSY");
    playAudioTrack(7);
    return false;
  }

  // 2. Chống gửi quá nhanh (> 5 lệnh/giây)
  uint32_t now = millis();
  uint8_t oldest = (rateIdx + 1) % 5;
  if (now - cmdTimestamps[oldest] < 1000) {
    sendAck(cmdId, false, "E_RATE_LIMITED");
    playAudioTrack(7);
    return false;
  }
  rateIdx = (rateIdx + 1) % 5;
  cmdTimestamps[rateIdx] = now;

  // 3. Phân luồng TV vs Điều hòa
  if (deviceId == "ac_living" || action.startsWith("AC_") || params.containsKey("temp")) {
    int temp = params["temp"] | 24;
    bool power = params["power"] | (action != "POWER_OFF");
    irac.next.protocol = decode_type_t::PANASONIC_AC;
    irac.next.power = power;
    irac.next.degrees = temp;
    irac.sendAc();
    Serial.printf("[IR AC] Đã phát lệnh điều hòa Panasonic: Power=%d, Temp=%d\n", power, temp);
  } else {
    // Tivi LG mã mẫu
    uint32_t code = 0x20DF10EF; // POWER
    if (action == "VOL_UP") code = 0x20DF40BF;
    else if (action == "VOL_DOWN") code = 0x20DFC03F;
    else if (action == "MUTE") code = 0x20DF906F;
    else if (action == "CH_VTV1") code = 0x20DF00FF;
    else if (action == "CH_VTV3") code = 0x20DF08F7;
    else if (action == "APP_YOUTUBE") code = 0x20DF58A7;

    irsend.sendNEC(code, 32);
    Serial.printf("[IR TV] Đã phát mã NEC: 0x%08X (Lệnh: %s)\n", code, action.c_str());
  }

  if (recentCmdIds.size() >= 16) recentCmdIds.erase(recentCmdIds.begin());
  recentCmdIds.push_back(cmdId);

  sendAck(cmdId, true);
  playAudioTrack(1); // "Đã nhận lệnh"
  wakeScreen();
  drawOled("Lenh IR: OK", action);
  return true;
}

// ================= WEBSOCKET SUPABASE REALTIME =================
void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    wsConnected = true;
    lastHbReply = millis();
    Serial.println("[WSS] Đã nối WebSocket! Gửi join channel...");
    // Join channel realtime:board:<boardId>
    String joinMsg = "[\"1\",\"1\",\"realtime:board:" + boardId + "\",\"phx_join\",{\"config\":{\"broadcast\":{\"self\":false},\"private\":true},\"access_token\":\"" + boardToken + "\"}]";
    wsClient.sendTXT(joinMsg);
  } else if (type == WStype_DISCONNECTED) {
    wsConnected = false;
    wsChannelJoined = false;
    ledState = LED_ST_WIFI_NO_INTERNET;
  } else if (type == WStype_TEXT) {
    String text = String((char*)payload);
    JsonDocument doc;
    if (deserializeJson(doc, text) == DeserializationError::Ok && doc.is<JsonArrayConst>()) {
      JsonArrayConst arr = doc.as<JsonArrayConst>();
      String topic = arr[2].as<String>();
      String event = arr[3].as<String>();
      JsonObjectConst pObj = arr[4].as<JsonObjectConst>();

      if (topic == "phoenix" && event == "phx_reply") {
        lastHbReply = millis();
      } else if (event == "phx_reply" && topic.startsWith("realtime:board:")) {
        wsChannelJoined = true;
        ledState = LED_ST_ONLINE;
        drawOled("Online", "San sang nhan lenh");
        sendBoardStatus();
      } else if (event == "broadcast") {
        String bEvt = pObj["event"] | "";
        JsonObjectConst bPayload = pObj["payload"].as<JsonObjectConst>();
        if (bEvt == "command") {
          sendIrCommand(bPayload["commandId"] | "", bPayload["deviceId"] | "", bPayload["action"] | "", bPayload["params"].as<JsonObjectConst>());
        }
      }
    }
  }
}

// ================= KHỞI ĐỘNG HỆ THỐNG =================
void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(CPU_FREQ_MHZ); // Hạ xung 80MHz tiết kiệm điện (Spec 5.2)

  pinMode(PIN_RGB_R, OUTPUT);
  pinMode(PIN_RGB_G, OUTPUT);
  pinMode(PIN_RGB_B, OUTPUT);
  setRgb(false, false, false);

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_IR_RECV_VCC, OUTPUT);
  digitalWrite(PIN_IR_RECV_VCC, LOW); // Tắt mắt thu ban đầu

  pinMode(PIN_BATTERY_ADC, INPUT);
  pinMode(PIN_USB_ADC, INPUT);

  irsend.begin();
  oled.init();
  oled.flipScreenVertically();
  oled.setFont(ArialMT_Plain_10);
  drawOled("Khoi dong...");

  dfSerial.begin(9600, SERIAL_8N1, PIN_DFPLAYER_RX, PIN_DFPLAYER_TX);
  if (dfPlayer.begin(dfSerial, false, true)) {
    dfPlayer.volume(26);
  }

  // Tải cấu hình WiFi và Supabase từ NVS
  prefs.begin("smarthome", false);
  boardId = prefs.getString("boardId", "board_living_01");
  boardToken = prefs.getString("boardToken", "");
  wifiSsid = prefs.getString("wifiSsid", "");
  wifiPass = prefs.getString("wifiPass", "");
  supabaseUrl = prefs.getString("supaUrl", "");
  supabaseAnonKey = prefs.getString("supaAnon", "");

  if (wifiSsid.isEmpty()) {
    drawOled("Giu nut 10s", "de cai dat WiFi");
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
    drawOled("Dang ket noi WiFi...", wifiSsid);
  }
}

// ================= VÒNG LẶP CHÍNH =================
void loop() {
  unsigned long now = millis();

  // 1. Quản lý nút nhấn đa năng (Spec 0.5)
  static bool lastBtn = HIGH;
  static unsigned long btnPressStart = 0;
  bool btn = digitalRead(PIN_BUTTON);
  if (btn == LOW && lastBtn == HIGH) {
    btnPressStart = now;
    wakeScreen();
  } else if (btn == HIGH && lastBtn == LOW) {
    unsigned long duration = now - btnPressStart;
    if (duration >= 20000) {
      // Giữ 20s: Reset gốc
      playAudioTrack(9);
      drawOled("KHOI PHUC GOC", "Dang reset...");
      prefs.clear();
      delay(2000);
      ESP.restart();
    } else if (duration >= 10000) {
      // Giữ 10s: Cài đặt WiFi
      playAudioTrack(3);
      drawOled("CHEDO CAIDAT", "Noi AP ESP32_Setup");
    } else if (duration >= 5000) {
      // Giữ 5s: Học lệnh từ nút
      playAudioTrack(4);
      drawOled("HOC LENH", "Bam remote 2 lan");
    } else {
      wakeScreen();
    }
  }
  lastBtn = btn;

  // 2. Tự tắt OLED sau 10s (Spec 5.3)
  if (screenPowered && !oledLockAwake && (now - lastScreenWake >= OLED_SLEEP_TIMEOUT_MS)) {
    screenPowered = false;
    oled.displayOff();
  }

  // 3. LED RGB nhấp nháy
  if (ledState == LED_ST_ONLINE && now - lastLedToggle >= (ledStep ? 2900 : 100)) {
    lastLedToggle = now; ledStep = !ledStep; setRgb(false, ledStep == 1, false);
  } else if (ledState == LED_ST_NO_WIFI && now - lastLedToggle >= 200) {
    lastLedToggle = now; ledStep = !ledStep; setRgb(ledStep == 1, false, false);
  }

  // 4. Kiểm tra mạng WiFi và WebSocket
  if (WiFi.status() == WL_CONNECTED) {
    static bool netInitialized = false;
    if (!netInitialized) {
      netInitialized = true;
      esp_wifi_set_ps(WIFI_PS_MIN_MODEM); // Tiết kiệm pin
      configTime(7 * 3600, 0, "pool.ntp.org");

      // Khởi tạo WebSocket kết nối Supabase
      if (!supabaseUrl.isEmpty() && !supabaseAnonKey.isEmpty()) {
        String host = supabaseUrl;
        host.replace("https://", ""); host.replace("http://", "");
        int idx = host.indexOf('/'); if (idx != -1) host = host.substring(0, idx);
        String path = "/realtime/v1/websocket?apikey=" + supabaseAnonKey + "&vsn=1.0.0";
        wsClient.onEvent(onWsEvent);
        wsClient.beginSSL(host.c_str(), 443, path.c_str());
      }
    }
    wsClient.loop();

    // Heartbeat Phoenix mỗi 25s
    if (wsConnected && now - lastHbSend >= REALTIME_HEARTBEAT_MS) {
      hbSeq++;
      String hb = "[\"1\",\"hb_" + String(hbSeq) + "\",\"phoenix\",\"heartbeat\",{}]";
      wsClient.sendTXT(hb);
      lastHbSend = now;
    }
  }

  // 5. Cập nhật pin & gửi báo cáo mỗi 5 phút
  static unsigned long lastBatCheck = 0;
  if (now - lastBatCheck >= 5000) {
    lastBatCheck = now;
    readBattery();
  }
  static unsigned long lastRep = 0;
  if (now - lastRep >= STATUS_REPORT_INTERVAL_MS) {
    lastRep = now;
    sendBoardStatus();
  }

  delay(10);
}
