#include "Provisioning.h"

#include <Arduino.h>
#include <stdio.h>

#include "Board.h"
#include "config.h"

void Provisioning::begin(SecureStore& store) { store_ = &store; }

void Provisioning::update(uint32_t nowMs) {
    if (pairing_ && nowMs - startMs_ >= PAIRING_CODE_TTL_MS) {
        log_i("Ma ghep doi het han");
        cancelPairing();
    }
}

bool Provisioning::isPaired() const { return store_ != nullptr && store_->hasBoardToken(); }

void Provisioning::startPairing(uint32_t nowMs) {
    snprintf(code_, sizeof(code_), "%06lu", static_cast<unsigned long>(boardRandom() % 1000000UL));
    startMs_ = nowMs;
    pairing_ = true;
}

void Provisioning::cancelPairing() {
    pairing_ = false;
    code_[0] = '\0';
}

bool Provisioning::completePairing(const char* boardToken, const char* boardId, const char* homeId) {
    if (store_ == nullptr || !store_->saveBoardToken(boardToken) || !store_->saveBoardId(boardId) ||
        !store_->saveHomeId(homeId)) {
        return false;
    }
    cancelPairing();
    return true;
}

void Provisioning::factoryReset() {
    cancelPairing();
    if (store_ != nullptr) {
        store_->clearAll();
    }
}
