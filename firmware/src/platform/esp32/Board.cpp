#include "modules/Board.h"

#include <Arduino.h>
#include <Wire.h>
#include <esp_random.h>

#include "config.h"

void boardBegin() {
    Serial.begin(115200);
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);
    analogSetPinAttenuation(PIN_USB_SENSE, ADC_11db);
}

uint32_t boardRandom() { return esp_random(); }

void boardRestart() { ESP.restart(); }
