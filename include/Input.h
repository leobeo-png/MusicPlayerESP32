#pragma once
// Rotary encoder (quadrature state machine in ISR, bounce-proof) + debounced button FSM.
#include <Arduino.h>

enum class InputEvent : uint8_t {
    None,
    Rotate,       // value = detents (+cw / -ccw), button not held
    RotateHeld,   // rotated while button held (suppresses the click on release)
    Click,        // short press released, no rotation
    LongPress,    // held >= LONG_PRESS_MS, no rotation (fires once while held)
};

struct InputMsg { InputEvent ev; int value; };

class Input {
public:
    void begin();
    // Call often (every few ms). Returns one event per call; call until None.
    InputMsg poll();
    bool buttonHeld() const { return btnStable_; }

private:
    static void IRAM_ATTR isr();
    static volatile int32_t count_;
    static volatile uint8_t state_;

    int32_t lastCount_ = 0;
    bool btnStable_ = false, btnRaw_ = false;
    uint32_t btnChangeAt_ = 0, btnDownAt_ = 0;
    bool rotatedWhileHeld_ = false, longFired_ = false;
    bool clickPending_ = false, longPending_ = false;
};
