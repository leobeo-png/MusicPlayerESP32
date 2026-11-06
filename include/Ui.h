#pragma once
// All TFT drawing. Runs only in the Arduino loop task (core 1); draws only what changed.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Player.h"
#include "MusicLibrary.h"

class Ui {
public:
    Ui(TFT_eSPI& tft, MusicLibrary& lib) : tft_(tft), lib_(lib) {}

    void begin();                    // panel + backlight (call before SD so errors can be shown)
    void loadFont();                 // after SD is mounted
    void clear();
    void message(const char* line1, const char* line2 = nullptr);  // full-screen notice

    void render(const PlayerStatus& s, uint8_t batteryPct);
    void showVolume() { volShownAt_ = millis() | 1; }

    // Browse mode: list cursor moves independently of the playing track.
    bool browsing() const { return browsing_; }
    void browseStart(int32_t from);
    void browseMove(int delta);
    int32_t browseEnd();             // returns selected index

    // Power
    void touch();                    // user activity: wake / undim
    void updatePower();              // dim -> off timeline
    bool awake() const { return state_ != Scr::Off; }
    uint32_t idleMs() const { return millis() - lastTouch_; }
    void sleepPanel();

private:
    enum class Scr : uint8_t { On, Dim, Off };
    void setBacklight(uint8_t level);
    void invalidate();

    void drawHeader(const PlayerStatus& s);
    void drawMeta(const PlayerStatus& s);
    void drawTime(const PlayerStatus& s);
    void drawProgress(const PlayerStatus& s);
    void drawPlayIcon(bool paused);
    void drawVolume(uint8_t v, bool visible);
    void drawBattery(uint8_t pct);
    void drawList(int32_t highlight);

    TFT_eSPI& tft_;
    MusicLibrary& lib_;
    int W_ = 240, H_ = 320;

    // last drawn state (-1 / sentinel = force redraw)
    uint32_t metaSeq_ = UINT32_MAX;
    int32_t  track_ = -2, listTop_ = -1, listHi_ = -2;
    int32_t  posSec_ = -1, durSec_ = -1, barW_ = -1;
    int8_t   paused_ = -1, vol_ = -1, bat_ = -1;
    bool     volVisible_ = false;
    uint32_t volShownAt_ = 0;

    bool     browsing_ = false;
    int32_t  cursor_ = 0;

    Scr      state_ = Scr::On;
    uint32_t lastTouch_ = 0;
};
