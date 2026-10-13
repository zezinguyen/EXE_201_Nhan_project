/*
 * =========================================================================
 * ĐỒ ÁN NHÀ THÔNG MINH - HỆ THỐNG ĐIỀU KHIỂN TIVI BẰNG GIỌNG NÓI QUA ZALO APP
 * Vi điều khiển: ESP32 (hoặc ESP8266)
 * Chức năng: 
 *   1. Kết nối WiFi gia đình, chạy WebServer REST API.
 *   2. Nhận lệnh từ Zalo Mini App (Bật/Tắt TV, Âm lượng, Kênh TV, Kênh YouTube).
 *   3. Phát tín hiệu Hồng ngoại (IR LED) điều khiển Smart TV.
 *   4. Xử lý mở kênh YouTube trực tiếp trên Smart TV.
 * =========================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRac.h>

// ================= CẤU HÌNH WIFI =================
const char* ssid = "YOUR_WIFI_SSID";         // Thay tên WiFi nhà bạn
const char* password = "YOUR_WIFI_PASSWORD"; // Thay mật khẩu WiFi nhà bạn

// ================= CẤU HÌNH PHẦN CỨNG =================
const uint16_t kIrLedPin = 4; // Chân GPIO kết nối cực dương (Anode) của LED hồng ngoại qua trở 220 Ohm
const int statusLedPin = 2;   // LED tích hợp báo trạng thái trên ESP32

IRsend irsend(kIrLedPin);
IRac ac(kIrLedPin);
WebServer server(80);

// Hàm gửi phản hồi kèm Header CORS (Bắt buộc để Zalo Mini App gọi không bị chặn)
void sendCorsHeader() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

// Xử lý OPTIONS request (CORS Preflight)
void handleOptions() {
  sendCorsHeader();
  server.send(204);
}

// Xử lý kiểm tra kết nối (Ping Test)
void handlePing() {
  sendCorsHeader();
  String response = "{\"status\":\"online\",\"device\":\"ESP32_SmartHome_TV\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  server.send(200, "application/json", response);
}

// Hàm chuyển đổi chuỗi mã HEX (ví dụ "0x20DF10EF") sang số nguyên 64-bit uint64_t
uint64_t parseHex(String hexStr) {
  hexStr.trim();
  if (hexStr.startsWith("0x") || hexStr.startsWith("0X")) {
    hexStr = hexStr.substring(2);
  }
  uint64_t result = 0;
  for (int i = 0; i < hexStr.length(); i++) {
    char c = hexStr.charAt(i);
    int value = 0;
    if (c >= '0' && c <= '9') value = c - '0';
    else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
    else break;
    result = (result << 4) | value;
  }
  return result;
}

// Xử lý lệnh nhận từ Zalo Mini App
void handleCommand() {
  sendCorsHeader();
  
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"Phương thức yêu cầu phải là POST\"}");
    return;
  }

  String body = server.arg("plain");
  Serial.println("\n--- [LỆNH MỚI TỪ ZALO MINI APP] ---");
  Serial.println(body);

  // Phân tích dữ liệu JSON đơn giản
  String action = "";
  String irCodeStr = "";
  String channelQuery = "";
  String title = "";
  String device = "";
  String acBrand = "Panasonic";
  int temp = 26;

  int actionIdx = body.indexOf("\"action\":\"");
  if (actionIdx != -1) {
    int end = body.indexOf("\"", actionIdx + 10);
    action = body.substring(actionIdx + 10, end);
  }

  int deviceIdx = body.indexOf("\"device\":\"");
  if (deviceIdx != -1) {
    int end = body.indexOf("\"", deviceIdx + 10);
    device = body.substring(deviceIdx + 10, end);
  }

  int acBrandIdx = body.indexOf("\"acBrand\":\"");
  if (acBrandIdx != -1) {
    int end = body.indexOf("\"", acBrandIdx + 11);
    acBrand = body.substring(acBrandIdx + 11, end);
  }

  int tempIdx = body.indexOf("\"temp\":");
  if (tempIdx != -1) {
    int end = body.indexOf(",", tempIdx + 7);
    if (end == -1) end = body.indexOf("}", tempIdx + 7);
    if (end != -1) {
      temp = body.substring(tempIdx + 7, end).toInt();
    }
  }

  int irIdx = body.indexOf("\"irCode\":\"");
  if (irIdx != -1) {
    int end = body.indexOf("\"", irIdx + 10);
    irCodeStr = body.substring(irIdx + 10, end);
  }

  int channelIdx = body.indexOf("\"channelQuery\":\"");
  if (channelIdx != -1) {
    int end = body.indexOf("\"", channelIdx + 16);
    channelQuery = body.substring(channelIdx + 16, end);
  }

  int titleIdx = body.indexOf("\"title\":\"");
  if (titleIdx != -1) {
    int end = body.indexOf("\"", titleIdx + 9);
    title = body.substring(titleIdx + 9, end);
  }

  // 1. XỬ LÝ LỆNH ĐIỀU HÒA (A/C)
  if (device == "ac" || action.startsWith("AC_")) {
    digitalWrite(statusLedPin, HIGH);

    decode_type_t protocol = decode_type_t::PANASONIC_AC;
    String brandLower = acBrand;
    brandLower.toLowerCase();

    if (brandLower.indexOf("daikin") != -1) protocol = decode_type_t::DAIKIN;
    else if (brandLower.indexOf("panasonic") != -1) protocol = decode_type_t::PANASONIC_AC;
    else if (brandLower.indexOf("lg") != -1) protocol = decode_type_t::LG;
    else if (brandLower.indexOf("samsung") != -1) protocol = decode_type_t::SAMSUNG_AC;
    else if (brandLower.indexOf("casper") != -1) protocol = decode_type_t::GREE;
    else if (brandLower.indexOf("gree") != -1) protocol = decode_type_t::GREE;
    else if (brandLower.indexOf("midea") != -1) protocol = decode_type_t::COOLIX;
    else if (brandLower.indexOf("mitsubishi") != -1) protocol = decode_type_t::MITSUBISHI_AC;
    else if (brandLower.indexOf("toshiba") != -1) protocol = decode_type_t::TOSHIBA_AC;
    else protocol = decode_type_t::PANASONIC_AC;

    bool isPowerOn = (action == "AC_POWER_ON" || action == "AC_SET_TEMP");
    if (action == "AC_POWER_OFF") isPowerOn = false;

    int setDegrees = (temp >= 16 && temp <= 30) ? temp : 26;

    ac.next.protocol = protocol;
    ac.next.power = isPowerOn;
    ac.next.mode = stdAc::opmode_t::kCool;
    ac.next.degrees = setDegrees;
    ac.next.fanspeed = stdAc::fanspeed_t::kAuto;
    ac.sendAc();

    digitalWrite(statusLedPin, LOW);
    Serial.printf(">> [ĐIỀU HÒA] Hãng: %s | Nguồn: %s | Nhiệt độ: %d°C\n",
                  acBrand.c_str(), isPowerOn ? "BẬT" : "TẮT", setDegrees);

    String responseJson = "{\"success\":true,\"message\":\"ESP32 đã phát lệnh điều hòa [" + title + "] thành công!\",\"device\":\"ac\"}";
    server.send(200, "application/json", responseJson);
    return;
  }

  // 2. Phát xung hồng ngoại IR tới Tivi
  uint64_t irCode = parseHex(irCodeStr);
  if (irCode != 0) {
    digitalWrite(statusLedPin, HIGH);
    
    // Mặc định phát chuẩn NEC 32-bit (Tương thích hầu hết TV LG, Casper, TCL, các dòng Android TV)
    // Nếu bạn dùng TV Samsung hoặc Sony, có thể đổi sang irsend.sendSAMSUNG(irCode, 32) hoặc irsend.sendSony(irCode, 12)
    irsend.sendNEC(irCode, 32);
    delay(50);
    
    digitalWrite(statusLedPin, LOW);
    Serial.printf(">> Đã phát xung IR HEX: 0x%llX tới Tivi\n", irCode);
  }

  // 3. Xử lý mở kênh YouTube cụ thể
  if (action == "YOUTUBE_SEARCH" && channelQuery.length() > 0) {
    Serial.printf(">> [YOUTUBE] Yêu cầu chuyển đến kênh/video: \"%s\"\n", channelQuery.c_str());
    // Mở YouTube trên TV bằng mã IR:
    irsend.sendNEC(0x20DF58A7, 32);
  }

  // Trả kết quả JSON về cho Zalo App
  String responseJson = "{\"success\":true,\"message\":\"ESP32 đã phát mã IR [" + irCodeStr + "] thành công!\",\"action\":\"" + action + "\"}";
  server.send(200, "application/json", responseJson);
}

void setup() {
  Serial.begin(115200);
  pinMode(statusLedPin, OUTPUT);
  digitalWrite(statusLedPin, LOW);

  // Khởi động module phát hồng ngoại IR TV và Điều hòa
  irsend.begin();
  ac.begin();

  // Kết nối WiFi
  Serial.printf("\nĐang kết nối WiFi: %s ...\n", ssid);
  WiFi.begin(ssid, password);

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 30) {
    delay(500);
    Serial.print(".");
    digitalWrite(statusLedPin, !digitalRead(statusLedPin));
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(statusLedPin, HIGH); // Sáng đèn khi đã có WiFi
    Serial.println("\n WiFi ĐÃ KẾT NỐI THÀNH CÔNG!");
    Serial.print(" ĐỊA CHỈ IP ESP32 CỦA BẠN LÀ: http://");
    Serial.println(WiFi.localIP());
    Serial.println(">> Hãy nhập địa chỉ IP này vào mục Cài đặt trên Zalo Mini App!");
  } else {
    Serial.println("\n Không thể kết nối WiFi. Vui lòng kiểm tra lại SSID/Mật khẩu!");
  }

  // Đăng ký các Endpoint WebServer REST API
  server.on("/api/ping", HTTP_OPTIONS, handleOptions);
  server.on("/api/ping", HTTP_GET, handlePing);
  server.on("/api/tv/command", HTTP_OPTIONS, handleOptions);
  server.on("/api/tv/command", HTTP_POST, handleCommand);

  server.begin();
  Serial.println(">> WebServer REST API đã sẵn sàng phục vụ lệnh từ Zalo Mini App.");
}

void loop() {
  server.handleClient();
}
