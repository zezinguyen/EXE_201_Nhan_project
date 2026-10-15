// Test trên mạch thật: kiểm tra lần lượt từng linh kiện đã nối đúng chưa.
// Màn hình OLED hướng dẫn từng bước; nút trên mạch dùng để chuyển bước:
//   Bấm ngắn   = linh kiện chạy đúng, sang bước tiếp
//   Giữ >= 2s  = linh kiện có vấn đề (ghi lỗi), sang bước tiếp
// Thứ tự: OLED + nút -> LED RGB -> mắt thu IR -> LED IR -> DFPlayer + loa -> đo pin/USB.
// Chạy: pio test -e esp32dev -f test_device_hardware -v   (-v để thấy hướng dẫn trên Serial)

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <ButtonDetector.h>
#include <Wire.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "config.h"
#include "modules/AudioPlayer.h"
#include "modules/Board.h"
#include "modules/IrLearner.h"
#include "modules/IrSender.h"

namespace {
constexpr uint32_t kFailHoldMs = 2000;          // Giữ nút lâu hơn mốc này = báo lỗi
constexpr uint32_t kScreenRefreshMs = 200;
constexpr uint32_t kLedStepMs = 800;
constexpr uint32_t kIrSendIntervalMs = 1000;
constexpr uint32_t kAudioGapMs = 1000;           // Nghỉ giữa hai file âm thanh
constexpr uint16_t kAudioLastTrack = 9;          // Phát vòng 0001..0009.mp3
constexpr uint16_t kBatteryMinMv = 3000;         // Ngoài khoảng này: cầu chia/dây đo pin sai
constexpr uint16_t kBatteryMaxMv = 4300;
constexpr uint8_t kTestCount = 6;
const char kTestProtocol[] = "NEC";
constexpr uint64_t kTestCode = 0x20DF10EF;
constexpr uint16_t kTestBits = 32;

const uint32_t kHoldLevelsMs[] = {kFailHoldMs};

Adafruit_SSD1306 screen(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
ButtonDetector button(BUTTON_DEBOUNCE_MS, kHoldLevelsMs, 1);
IrLearner learner;
IrSender sender;
AudioPlayer audio;

bool oledReady = false;
uint8_t testNumber = 0;
char lastSerialText[200] = {};

enum class Verdict : uint8_t { Ok, Fail };

// Vẽ một trang: dòng tiêu đề (nền trắng) + tối đa 6 dòng nội dung (21 ký tự/dòng, không dấu).
// Đồng thời in ra Serial khi nội dung đổi, để vẫn test được khi OLED hỏng.
void showPage(const char* title, const char* lines[], uint8_t count) {
    char serialText[sizeof(lastSerialText)];
    int used = snprintf(serialText, sizeof(serialText), "[%u/%u] %s", testNumber, kTestCount, title);
    for (uint8_t i = 0; i < count && used > 0 && used < static_cast<int>(sizeof(serialText)); i++) {
        used += snprintf(serialText + used, sizeof(serialText) - used, " | %s", lines[i]);
    }
    if (strcmp(serialText, lastSerialText) != 0) {
        strncpy(lastSerialText, serialText, sizeof(lastSerialText) - 1);
        Serial.println(serialText);
    }

    if (!oledReady) {
        return;
    }
    screen.clearDisplay();
    screen.fillRect(0, 0, OLED_WIDTH, 9, SSD1306_WHITE);
    screen.setTextColor(SSD1306_BLACK);
    screen.setCursor(1, 1);
    screen.printf("%u/%u %s", testNumber, kTestCount, title);
    screen.setTextColor(SSD1306_WHITE);
    for (uint8_t i = 0; i < count && i < 6; i++) {
        screen.setCursor(0, 12 + i * 9);
        screen.print(lines[i]);
    }
    screen.display();
}

bool buttonPressedRaw() { return (digitalRead(PIN_BUTTON) == LOW) == BUTTON_ACTIVE_LOW; }

// Chạy tick() liên tục cho tới khi người test bấm nút. tick vẽ màn hình + điều khiển linh kiện.
Verdict waitForVerdict(void (*tick)(uint32_t nowMs)) {
    for (;;) {
        const uint32_t nowMs = millis();
        const ButtonEvent event = button.update(buttonPressedRaw(), nowMs);
        if (event.type == PressType::Short) {
            return Verdict::Ok;
        }
        if (event.type == PressType::Hold) {
            return Verdict::Fail;
        }
        tick(nowMs);
        delay(LOOP_IDLE_MS);
    }
}

bool refreshDue(uint32_t nowMs) {
    static uint32_t lastMs = 0;
    if (nowMs - lastMs < kScreenRefreshMs) {
        return false;
    }
    lastMs = nowMs;
    return true;
}

// ---------------------------------------------------------------- 1. OLED + nút
void tickOled(uint32_t nowMs) {
    if (!refreshDue(nowMs)) {
        return;
    }
    const uint32_t held = button.heldMs(nowMs);
    const char* buttonText = !button.isPressed() ? "Nut: dang nha"
                             : held >= kFailHoldMs ? "Nut: giu -> BAO LOI"
                                                   : "Nut: DANG BAM";
    const char* lines[] = {"OLED da ket noi!", buttonText, "", "Bam ngan: OK, tiep", "Giu 2s: bao loi"};
    showPage("OLED + NUT", lines, 5);
}

void testOledAndButton() {
    testNumber = 1;
    oledReady = screen.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS);
    if (!oledReady) {
        Serial.printf("Khong thay OLED o dia chi 0x%02X (SDA=%u, SCL=%u). Bam nut de tiep.\n", OLED_I2C_ADDRESS,
                      PIN_OLED_SDA, PIN_OLED_SCL);
    }
    const Verdict verdict = waitForVerdict(tickOled);
    TEST_ASSERT_TRUE_MESSAGE(oledReady, "OLED khong phan hoi I2C: kiem tra VCC 3V3, GND, SDA/SCL, dia chi 0x3C/0x3D");
    TEST_ASSERT_TRUE_MESSAGE(verdict == Verdict::Ok, "Nguoi test bao loi OLED (chu mo, sai, nhieu?)");
}

// ---------------------------------------------------------------- 2. LED RGB
struct LedStep {
    const char* name;
    bool red, green, blue;
};
const LedStep kLedSteps[] = {
    {"DO", true, false, false},       {"XANH LA", false, true, false}, {"XANH DUONG", false, false, true},
    {"TRANG (ca 3)", true, true, true}, {"TAT", false, false, false},
};

void writeLed(bool red, bool green, bool blue) {
    digitalWrite(PIN_LED_RED, red == STATUS_LED_ACTIVE_HIGH ? HIGH : LOW);
    digitalWrite(PIN_LED_GREEN, green == STATUS_LED_ACTIVE_HIGH ? HIGH : LOW);
    digitalWrite(PIN_LED_BLUE, blue == STATUS_LED_ACTIVE_HIGH ? HIGH : LOW);
}

void tickLed(uint32_t nowMs) {
    static uint32_t stepStartMs = 0;
    static uint8_t step = 0;
    if (nowMs - stepStartMs >= kLedStepMs) {
        stepStartMs = nowMs;
        step = (step + 1) % (sizeof(kLedSteps) / sizeof(kLedSteps[0]));
        writeLed(kLedSteps[step].red, kLedSteps[step].green, kLedSteps[step].blue);
    }
    if (!refreshDue(nowMs)) {
        return;
    }
    char current[24];
    snprintf(current, sizeof(current), "Dang sang: %s", kLedSteps[step].name);
    const char* lines[] = {current, "Dung thu tu: Do >", "Xanh la > Xanh duong", "> Trang > Tat", "Bam: OK  Giu 2s: loi"};
    showPage("LED RGB", lines, 5);
}

void testRgbLed() {
    testNumber = 2;
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);
    const Verdict verdict = waitForVerdict(tickLed);
    writeLed(false, false, false);
    TEST_ASSERT_TRUE_MESSAGE(verdict == Verdict::Ok,
                             "LED RGB sai: khong sang = sai chan chung/tro; sai mau = sai thu tu chan R/G/B "
                             "hoac sai STATUS_LED_ACTIVE_HIGH");
}

// ---------------------------------------------------------------- 3. Mắt thu IR
uint16_t irReceivedCount = 0;
char irLastProtocol[24] = "-";
char irLastCode[24] = "";

// Mắt thu dùng đúng IrLearner của firmware. Mỗi lần thu được một gói thì ghi lại;
// phiên học tự kết thúc (khớp/lệch/hết giờ) thì bật lại để thu tiếp.
void pollReceiver(uint32_t nowMs, bool* echoOut) {
    const LearnEvent event = learner.update(nowMs);
    if (event == LearnEvent::FirstCaptured || event == LearnEvent::Mismatch || event == LearnEvent::Success) {
        irReceivedCount++;
        const IrCapture& capture = learner.result();
        strncpy(irLastProtocol, capture.protocol, sizeof(irLastProtocol) - 1);
        if (strcmp(capture.protocol, "UNKNOWN") == 0) {
            snprintf(irLastCode, sizeof(irLastCode), "raw %u xung", capture.rawLength);
        } else {
            snprintf(irLastCode, sizeof(irLastCode), "0x%llX %ub", static_cast<unsigned long long>(capture.value),
                     capture.bits);
        }
        if (echoOut != nullptr && capture.value == kTestCode) {
            *echoOut = true;
        }
    }
    if (!learner.isActive()) {
        learner.start(nowMs);
    }
}

void tickReceiver(uint32_t nowMs) {
    pollReceiver(nowMs, nullptr);
    if (!refreshDue(nowMs)) {
        return;
    }
    char count[24];
    snprintf(count, sizeof(count), "Da nhan: %u goi", irReceivedCount);
    char protocol[32];
    snprintf(protocol, sizeof(protocol), "Ma: %s", irLastProtocol);
    const char* lines[] = {"Chia remote bat ky", "vao mat thu, bam phim", count, protocol, irLastCode,
                           "Bam: OK  Giu 2s: loi"};
    showPage("MAT THU IR", lines, 6);
}

void testIrReceiver() {
    testNumber = 3;
    learner.begin();
    learner.start(millis());
    const Verdict verdict = waitForVerdict(tickReceiver);
    learner.stop();
    TEST_ASSERT_TRUE_MESSAGE(verdict == Verdict::Ok, "Nguoi test bao loi mat thu IR");
    TEST_ASSERT_TRUE_MESSAGE(irReceivedCount > 0,
                             "Khong nhan duoc goi IR nao: kiem tra OUT -> PIN_IR_RX, VCC/GND (VS1838B: OUT-GND-VCC)");
}

// ---------------------------------------------------------------- 4. LED IR
uint16_t irSentCount = 0;
bool irEchoSeen = false;

void tickIrLed(uint32_t nowMs) {
    static uint32_t lastSendMs = 0;
    if (nowMs - lastSendMs >= kIrSendIntervalMs) {
        lastSendMs = nowMs;
        if (sender.sendCode(kTestProtocol, kTestCode, kTestBits)) {
            irSentCount++;
        }
    }
    pollReceiver(nowMs, &irEchoSeen);
    if (!refreshDue(nowMs)) {
        return;
    }
    char sent[24];
    snprintf(sent, sizeof(sent), "Da phat: %u lan", irSentCount);
    const char* lines[] = {sent,
                           "Soi LED bang camera",
                           "dt: thay nhay tim/trang",
                           irEchoSeen ? "Mat thu bat lai: CO" : "Mat thu bat lai: chua",
                           "(huong LED vao mat thu)",
                           "Bam: OK  Giu 2s: loi"};
    showPage("LED IR (phat)", lines, 6);
}

void testIrLed() {
    testNumber = 4;
    sender.begin();
    learner.start(millis());
    const Verdict verdict = waitForVerdict(tickIrLed);
    learner.stop();
    TEST_ASSERT_TRUE_MESSAGE(irSentCount > 0, "Thu vien khong phat duoc ma NEC");
    TEST_ASSERT_TRUE_MESSAGE(verdict == Verdict::Ok,
                             "LED IR khong sang: kiem tra 330R -> B S8050, E -> GND, C -> catot LED, 5V -> 22R -> "
                             "anot LED (chan dai)");
}

// ---------------------------------------------------------------- 5. DFPlayer + loa
uint16_t audioTrack = 0;

void tickAudio(uint32_t nowMs) {
    static uint32_t idleSinceMs = 0;
    static bool wasBusy = false;
    audio.update(nowMs);
    if (wasBusy && !audio.isBusy()) {
        idleSinceMs = nowMs;
    }
    wasBusy = audio.isBusy();
    if (audio.isReady() && !audio.isBusy() && nowMs - idleSinceMs >= kAudioGapMs) {
        audioTrack = audioTrack % kAudioLastTrack + 1;
        audio.play(static_cast<AudioTrack>(audioTrack));
        wasBusy = true;
    }
    if (!refreshDue(nowMs)) {
        return;
    }
    char status[24];
    const char* hint = "Nghe thay tieng?";
    if (audio.fault() == AudioFault::NoResponse) {
        snprintf(status, sizeof(status), "DFPlayer KHONG TL!");
        hint = "Kiem tra TX2/RX2, 3V3";
    } else if (audio.fault() == AudioFault::NoCard) {
        snprintf(status, sizeof(status), "KHONG THAY THE NHO!");
        hint = "The FAT32, /mp3/0001..";
    } else if (!audio.isReady()) {
        snprintf(status, sizeof(status), "Dang khoi dong...");
    } else {
        snprintf(status, sizeof(status), "Phat /mp3/%04u.mp3", audioTrack);
    }
    const char* lines[] = {status, hint, "", "", "Bam: OK  Giu 2s: loi"};
    showPage("DFPLAYER + LOA", lines, 5);
}

void testAudio() {
    testNumber = 5;
    const Verdict verdict = waitForVerdict(tickAudio);
    audio.stop();
    TEST_ASSERT_FALSE_MESSAGE(audio.fault() == AudioFault::NoResponse,
                              "DFPlayer khong tra loi UART: TX2(17)->RX DFPlayer, RX2(16)<-TX DFPlayer, VCC 3V3, GND chung");
    TEST_ASSERT_FALSE_MESSAGE(audio.fault() == AudioFault::NoCard,
                              "DFPlayer khong thay file: the FAT32, cam chat, co /mp3/0001.mp3 ...");
    TEST_ASSERT_TRUE_MESSAGE(verdict == Verdict::Ok,
                             "Khong nghe tieng: DAC_R/L -> dau vao PAM8403, PAM8403 5V, loa vao R+/R- (khong noi "
                             "R- xuong GND), AUDIO_VOLUME");
}

// ---------------------------------------------------------------- 6. Đo pin / USB
uint16_t batteryMv = 0;
uint16_t usbMv = 0;

void tickPower(uint32_t nowMs) {
    if (!refreshDue(nowMs)) {
        return;
    }
    const uint16_t batteryPinMv = analogReadMilliVolts(PIN_BATTERY_ADC);
    const uint16_t usbPinMv = analogReadMilliVolts(PIN_USB_SENSE);
    batteryMv = static_cast<uint16_t>(batteryPinMv * BATTERY_DIVIDER_RATIO);
    usbMv = usbPinMv;
    char battery[24];
    snprintf(battery, sizeof(battery), "Pin: %u mV", batteryMv);
    char batteryPin[24];
    snprintf(batteryPin, sizeof(batteryPin), " (chan VP %u mV)", batteryPinMv);
    char usb[24];
    snprintf(usb, sizeof(usb), "USB: %s (VN %u)", usbPinMv >= USB_SENSE_MIN_MV ? "CO" : "khong", usbPinMv);
    const char* lines[] = {battery, batteryPin, usb, "So voi dong ho do pin", "Cam/rut USB thu", "Bam: OK  Giu 2s: loi"};
    showPage("DO PIN / USB", lines, 6);
}

void testPowerSense() {
    testNumber = 6;
    const Verdict verdict = waitForVerdict(tickPower);
    TEST_ASSERT_TRUE_MESSAGE(batteryMv >= kBatteryMinMv && batteryMv <= kBatteryMaxMv,
                             "Dien ap pin ngoai 3.0-4.3V: kiem tra cau chia 100k/100k tu B+ vao VP (GPIO36)");
    TEST_ASSERT_TRUE_MESSAGE(verdict == Verdict::Ok, "Nguoi test bao loi do pin/USB (lech dong ho, USB khong doi?)");
}
}  // namespace

void setUp() {}
void tearDown() { delay(300); }  // Màn hình kịp đổi trước bước sau

void setup() {
    delay(2000);  // Chờ cổng Serial của bộ chạy test
    boardBegin();
    pinMode(PIN_BUTTON, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
    audio.begin();  // DFPlayer cần ~1.5s, khởi động sẵn từ đầu

    UNITY_BEGIN();
    RUN_TEST(testOledAndButton);
    RUN_TEST(testRgbLed);
    RUN_TEST(testIrReceiver);
    RUN_TEST(testIrLed);
    RUN_TEST(testAudio);
    RUN_TEST(testPowerSense);
    const int failures = UNITY_END();

    if (oledReady) {
        char result[24];
        snprintf(result, sizeof(result), "Loi: %d / %u", failures, kTestCount);
        const char* lines[] = {result, failures == 0 ? "Tat ca linh kien OK" : "Xem chi tiet o Serial"};
        testNumber = kTestCount;
        showPage("KET QUA", lines, 2);
    }
}

void loop() {}
