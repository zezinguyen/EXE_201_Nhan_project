#include "UserButton.h"

#include <Arduino.h>

#include "config.h"

namespace {
const uint32_t kHoldLevelsMs[] = {BUTTON_LEARN_HOLD_MS, BUTTON_SETUP_HOLD_MS, BUTTON_RESET_HOLD_MS};
// Chỉ số = mức giữ (0 = bấm ngắn)
const ButtonAction kActionByLevel[] = {
    ButtonAction::ShowStatus,
    ButtonAction::LearnCommand,
    ButtonAction::SetupMode,
    ButtonAction::FactoryReset,
};
}  // namespace

const char* buttonActionText(ButtonAction action) {
    switch (action) {
        case ButtonAction::None: return "";
        case ButtonAction::ShowStatus: return "Xem trang thai";
        case ButtonAction::LearnCommand: return "Hoc lenh";
        case ButtonAction::SetupMode: return "Cai dat WiFi";
        case ButtonAction::FactoryReset: return "Khoi phuc goc";
    }
    return "";
}

UserButton::UserButton() : detector_(BUTTON_DEBOUNCE_MS, kHoldLevelsMs, sizeof(kHoldLevelsMs) / sizeof(kHoldLevelsMs[0])) {}

void UserButton::begin(ButtonActionHandler handler) {
    handler_ = handler;
    pinMode(PIN_BUTTON, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
}

void UserButton::update(uint32_t nowMs) {
    const bool pressed = (digitalRead(PIN_BUTTON) == LOW) == BUTTON_ACTIVE_LOW;
    const ButtonEvent event = detector_.update(pressed, nowMs);
    if (event.type != PressType::None && handler_ != nullptr) {
        handler_(kActionByLevel[event.holdLevel]);
    }
}

ButtonAction UserButton::actionIfReleased(uint32_t nowMs) const {
    return isPressed() ? kActionByLevel[detector_.currentHoldLevel(nowMs)] : ButtonAction::None;
}
