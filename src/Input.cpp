#include "Input.h"
#include "Config.h"
#include <soc/gpio_struct.h>

volatile int32_t Input::count_ = 0;
volatile uint8_t Input::state_ = 0;

// Quadrature decoder (Ben Buxton's table). Invalid/bouncing transitions return to a
// known state without emitting a step, so no time-based debounce is needed.
#define R_START 0x0
#define DIR_CW  0x10
#define DIR_CCW 0x20

// Full-step: one emit per complete 00->11->00 cycle.
#define F_CW_FINAL 0x1
#define F_CW_BEGIN 0x2
#define F_CW_NEXT  0x3
#define F_CCW_BEGIN 0x4
#define F_CCW_FINAL 0x5
#define F_CCW_NEXT 0x6
static const uint8_t DRAM_ATTR kFull[7][4] = {
    {R_START,    F_CW_BEGIN,  F_CCW_BEGIN, R_START},
    {F_CW_NEXT,  R_START,     F_CW_FINAL,  R_START | DIR_CW},
    {F_CW_NEXT,  F_CW_BEGIN,  R_START,     R_START},
    {F_CW_NEXT,  F_CW_BEGIN,  F_CW_FINAL,  R_START},
    {F_CCW_NEXT, R_START,     F_CCW_BEGIN, R_START},
    {F_CCW_NEXT, F_CCW_FINAL, R_START,     R_START | DIR_CCW},
    {F_CCW_NEXT, F_CCW_FINAL, F_CCW_BEGIN, R_START},
};

// Half-step: emits at 00 and at 11.
#define H_CCW_BEGIN   0x1
#define H_CW_BEGIN    0x2
#define H_START_M     0x3
#define H_CW_BEGIN_M  0x4
#define H_CCW_BEGIN_M 0x5
static const uint8_t DRAM_ATTR kHalf[6][4] = {
    {H_START_M,           H_CW_BEGIN,    H_CCW_BEGIN,  R_START},
    {H_START_M | DIR_CCW, R_START,       H_CCW_BEGIN,  R_START},
    {H_START_M | DIR_CW,  H_CW_BEGIN,    R_START,      R_START},
    {H_START_M,           H_CCW_BEGIN_M, H_CW_BEGIN_M, R_START},
    {H_START_M,           H_START_M,     H_CW_BEGIN_M, R_START | DIR_CW},
    {H_START_M,           H_CCW_BEGIN_M, H_START_M,    R_START | DIR_CCW},
};

void IRAM_ATTR Input::isr() {
    const uint32_t in = GPIO.in;  // pins 26/27 < 32
    const uint8_t pins = (((in >> cfg::PIN_ENC_B) & 1) << 1) | ((in >> cfg::PIN_ENC_A) & 1);
    uint8_t s = cfg::ENC_HALF_STEP ? kHalf[state_ & 0x0F][pins] : kFull[state_ & 0x0F][pins];
    state_ = s;
    if (s & DIR_CW)  count_ = count_ + (cfg::ENC_REVERSE ? -1 : 1);
    if (s & DIR_CCW) count_ = count_ + (cfg::ENC_REVERSE ? 1 : -1);
}

void Input::begin() {
    static_assert(cfg::PIN_ENC_A < 32 && cfg::PIN_ENC_B < 32, "ISR reads GPIO.in (pins 0-31)");
    pinMode(cfg::PIN_ENC_A, INPUT_PULLUP);
    pinMode(cfg::PIN_ENC_B, INPUT_PULLUP);
    pinMode(cfg::PIN_ENC_SW, INPUT_PULLUP);  // was missing: GPIO14 floated
    attachInterrupt(digitalPinToInterrupt(cfg::PIN_ENC_A), isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(cfg::PIN_ENC_B), isr, CHANGE);
    btnRaw_ = btnStable_ = digitalRead(cfg::PIN_ENC_SW) == LOW;
    btnDownAt_ = millis();
    longFired_ = btnStable_;  // held at boot (rescan request) must not also fire LongPress/Click
}

InputMsg Input::poll() {
    const uint32_t now = millis();

    // ---- button debounce (20 ms) ----
    bool raw = digitalRead(cfg::PIN_ENC_SW) == LOW;
    if (raw != btnRaw_) { btnRaw_ = raw; btnChangeAt_ = now; }
    if (raw != btnStable_ && now - btnChangeAt_ >= 20) {
        btnStable_ = raw;
        if (raw) {                        // pressed
            btnDownAt_ = now;
            rotatedWhileHeld_ = longFired_ = false;
        } else if (!rotatedWhileHeld_ && !longFired_) {
            clickPending_ = true;         // act on release, so press+rotate never pauses
        }
    }
    if (btnStable_ && !rotatedWhileHeld_ && !longFired_ && now - btnDownAt_ >= cfg::LONG_PRESS_MS) {
        longFired_ = true;
        longPending_ = true;
    }

    // ---- encoder ----
    int32_t c = count_;
    int32_t d = c - lastCount_;
    if (d) {
        lastCount_ = c;
        if (btnStable_) { rotatedWhileHeld_ = true; return {InputEvent::RotateHeld, (int)d}; }
        return {InputEvent::Rotate, (int)d};
    }
    if (clickPending_) { clickPending_ = false; return {InputEvent::Click, 0}; }
    if (longPending_)  { longPending_ = false;  return {InputEvent::LongPress, 0}; }
    return {InputEvent::None, 0};
}
