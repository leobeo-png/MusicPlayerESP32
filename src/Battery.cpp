#include "Battery.h"
#include "Config.h"

void Battery::begin() {
    analogSetPinAttenuation(cfg::PIN_VBAT, ADC_11db);
    for (int i = 0; i < 8; ++i) analogReadMilliVolts(cfg::PIN_VBAT);  // first conversions are junk
    last_ = millis() - cfg::VBAT_PERIOD_MS;
}

// Typical 1-cell LiPo resting curve at light load (mV -> %).
uint8_t Battery::lipoPercent(uint16_t mv) {
    static const uint16_t v[] = {4200, 4100, 4000, 3900, 3800, 3750, 3700, 3650, 3600, 3500, 3300};
    static const uint8_t  p[] = { 100,   90,   80,   68,   55,   45,   35,   22,   12,    5,    0};
    if (mv >= v[0]) return 100;
    for (size_t i = 1; i < sizeof v / sizeof v[0]; ++i)
        if (mv >= v[i])
            return p[i] + (uint32_t)(mv - v[i]) * (p[i - 1] - p[i]) / (v[i - 1] - v[i]);
    return 0;
}

bool Battery::update() {
    uint32_t now = millis();
    if (now - last_ < cfg::VBAT_PERIOD_MS) return false;
    last_ = now;

    // analogReadMilliVolts uses the eFuse calibration -> far better than raw * 3.3 / 4095.
    uint32_t acc = 0;
    for (int i = 0; i < 16; ++i) acc += analogReadMilliVolts(cfg::PIN_VBAT);
    uint16_t mv = (uint16_t)(acc / 16 * cfg::VBAT_DIVIDER * cfg::VBAT_CAL);

    // Light IIR filter so audio-load ripple doesn't make the icon jump.
    const bool snap = !primed_ || abs((int)mv - (int)mv_) > 300;
    mv_ = snap ? mv : (uint16_t)((mv_ * 3u + mv) / 4u);
    primed_ = true;
    pct_ = lipoPercent(mv_);
    Serial.printf("[bat] %u mV (%u%%)\n", mv_, pct_);
    return true;
}
