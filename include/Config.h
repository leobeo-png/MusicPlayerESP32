#pragma once
// Single source of truth for pins and tunables.
// TFT pins (CS=15, DC=2, RST=EN, BL) stay in TFT_eSPI's User_Setup.h (see platformio.ini).
#include <Arduino.h>

namespace cfg {

// ---- Shared VSPI bus (TFT + SD) ----
constexpr int      PIN_SPI_SCK  = 18;
constexpr int      PIN_SPI_MISO = 19;
constexpr int      PIN_SPI_MOSI = 23;
constexpr int      PIN_SD_CS    = 5;
constexpr uint32_t SD_SPI_HZ    = 20000000;   // drop to 10 MHz if the card is flaky

// ---- I2S -> PCM5102A ----
constexpr int PIN_I2S_BCLK = 16;
constexpr int PIN_I2S_LRC  = 17;
constexpr int PIN_I2S_DOUT = 21;
constexpr int PIN_DAC_XSMT = -1;  // wire PCM5102 XSMT to a GPIO to mute while paused; -1 = not wired

// ---- Rotary encoder (bare encoder, internal pull-ups used) ----
constexpr int  PIN_ENC_A    = 26;
constexpr int  PIN_ENC_B    = 27;
constexpr int  PIN_ENC_SW   = 14;
constexpr bool ENC_REVERSE  = false;  // flip if clockwise turns volume down
constexpr bool ENC_HALF_STEP = false; // set true if one detent = half a quadrature cycle

// ---- Battery sense ----
constexpr int   PIN_VBAT      = 34;           // ADC1
constexpr float VBAT_DIVIDER  = 147.0f / 100.0f; // (R1 + R2) / R2, 47k top / 100k bottom
constexpr float VBAT_CAL = 4.075f / 3.62f;        // trim: multimeter_V / reported_V (see Serial log)
constexpr uint32_t VBAT_PERIOD_MS = 10000;

// ---- Files ----
constexpr const char* MUSIC_DIR  = "/music";
constexpr const char* INDEX_FILE = "/song_index.txt";
constexpr const char* SD_FONT    = "/fonts/ZenMaruGothic-Regular-16-minJap-noSmooth"; // no .vlw
constexpr bool RESCAN_ON_BOOT    = false; // rescan via Settings > Rescan library, or hold knob at power-on

// ---- Audio / UX ----
constexpr uint8_t  VOLUME_MAX       = 21;
constexpr uint8_t  VOLUME_DEFAULT   = 14;
constexpr uint32_t DIM_AFTER_MS     = 10000;  // backlight dims
constexpr uint32_t SCREEN_OFF_MS    = 30000;  // backlight off + panel sleep
constexpr uint8_t  BRIGHTNESS_FULL  = 255;
constexpr uint8_t  BRIGHTNESS_DIM   = 40;
constexpr uint32_t LONG_PRESS_MS    = 700;

// ---- Power ----
constexpr uint32_t CPU_MHZ = 160;   // 240 = stock. Try 80 once you've confirmed no stutter at 160.
constexpr uint32_t SLEEP_AFTER_PAUSE_MS = 10UL * 60 * 1000; // deep sleep when paused this long; 0 = never
constexpr uint16_t VBAT_CUTOFF_MV = 0;  // deep sleep below this (e.g. 3300). 0 = off until VBAT_CAL is trimmed

// ---- Tasks ----
constexpr int      AUDIO_CORE       = 0;
constexpr UBaseType_t AUDIO_PRIO    = 3;     // above Arduino loopTask (1)
constexpr uint32_t AUDIO_STACK      = 12288; // bytes; check high-water mark in Serial log

} // namespace cfg
