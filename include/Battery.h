#pragma once
#include <Arduino.h>

class Battery {
public:
    void begin();
    // Samples every VBAT_PERIOD_MS. Returns true when a new value is available.
    bool update();
    uint16_t millivolts() const { return mv_; }
    uint8_t percent() const { return pct_; }

private:
    static uint8_t lipoPercent(uint16_t mv);
    uint32_t last_ = 0;
    uint16_t mv_ = 0;
    uint8_t pct_ = 0;
    bool primed_ = false;
};
