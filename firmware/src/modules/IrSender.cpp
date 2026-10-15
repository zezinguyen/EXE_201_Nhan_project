#include "IrSender.h"

#include <IRac.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRutils.h>

#include "config.h"

namespace {
IRsend irSend(PIN_IR_TX);
IRac irAc(PIN_IR_TX);
}  // namespace

void IrSender::begin() { irSend.begin(); }

bool IrSender::sendCode(const char* protocol, uint64_t value, uint16_t bits) {
    const decode_type_t type = strToDecodeType(protocol);
    if (type == decode_type_t::UNKNOWN) {
        return false;
    }
    return irSend.send(type, value, bits);
}

bool IrSender::sendRaw(const uint16_t* durations, uint16_t length, uint16_t khz) {
    if (durations == nullptr || length == 0) {
        return false;
    }
    irSend.sendRaw(durations, length, khz);
    return true;
}

bool IrSender::sendAc(const AcRequest& request) {
    const decode_type_t type = strToDecodeType(request.protocol);
    if (!IRac::isProtocolSupported(type)) {
        return false;
    }
    stdAc::state_t state;
    IRac::initState(&state);
    state.protocol = type;
    state.power = request.power;
    state.degrees = request.temperatureC;
    state.celsius = true;
    state.mode = IRac::strToOpmode(request.mode, stdAc::opmode_t::kAuto);
    state.fanspeed = IRac::strToFanspeed(request.fan, stdAc::fanspeed_t::kAuto);
    return irAc.sendAc(state, nullptr);
}
