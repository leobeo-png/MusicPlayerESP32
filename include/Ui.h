#pragma once
// iPod-style UI. Runs only in the Arduino loop task (core 1); draws only what changed.
//   Lists:        rotate = move, hold+rotate = page, click = select, long press = back
//   Now Playing:  rotate = volume, hold+rotate = prev/next, click = play/pause, long press = back
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Player.h"
#include "MusicLibrary.h"
#include "Input.h"

class Ui {
public:
    Ui(TFT_eSPI& tft, MusicLibrary& lib, Player& player) : tft_(tft), lib_(lib), player_(player) {}

    void begin();     // panel, backlight, saved settings (before SD so errors can be shown)
    void loadFont();  // after SD is mounted
    void message(const char* line1, const char* line2 = nullptr);
    void showHome();  // initial screen stack: Main > Now Playing

    void handle(const InputMsg& m);
    void render(const PlayerStatus& s, uint8_t batPct, uint16_t batMv);

    // Power
    void updatePower();
    bool awake() const { return scr_ != Panel::Off; }
    void sleepPanel();
    void wake();
    bool sleepTimerDue() const { return sleepAt_ && (int32_t)(millis() - sleepAt_) >= 0; }

    // Rescan requested from Settings (survives the reboot that applies it)
    static bool takeRescanFlag();

private:
    enum class Screen : uint8_t { NowPlaying, Main, Songs, Folders, FolderSongs, Settings };
    enum class Panel : uint8_t { On, Dim, Off };
    struct View { Screen scr; int16_t sel, top, arg; };

    // navigation
    View& cur() { return stack_[depth_ - 1]; }
    void push(Screen s, int16_t arg = 0, int16_t sel = 0);
    void pop();
    void goNowPlaying();

    // list model
    int  itemCount(const View& v) const;
    void itemLabel(const View& v, int i, char* buf, size_t len) const;
    bool itemValue(const View& v, int i, char* buf, size_t len, const PlayerStatus& s) const;
    bool itemHasChild(const View& v, int i) const;
    int  itemSong(const View& v, int i) const;   // song index for song rows, else -1
    const char* title(const View& v) const;
    void select(int i);

    // drawing
    void clearBody();
    void drawHeader(const char* title, bool paused, uint8_t pct);
    void renderList(const PlayerStatus& s);
    void drawRow(const View& v, int row, int i, const PlayerStatus& s);
    void drawScrollbar(int count, int top);
    void renderNowPlaying(const PlayerStatus& s);
    void npMeta(const PlayerStatus& s);
    void npProgress(const PlayerStatus& s, bool full);
    void npTransport(bool paused);
    void npVolume(uint8_t v);
    void npStatus(const PlayerStatus& s);

    // settings
    void loadSettings();
    void saveSettings();
    void setBacklight(uint8_t level);
    uint32_t dimMs() const;
    uint32_t offMs() const;

    TFT_eSPI& tft_;
    MusicLibrary& lib_;
    Player& player_;
    int W_ = 240, H_ = 320;

    View stack_[6];
    uint8_t depth_ = 0;
    bool full_ = true;              // screen changed -> clear and redraw everything

    // header cache
    const char* hdrTitle_ = nullptr;
    int8_t hdrPaused_ = -1, hdrPct_ = -1;

    // list cache
    int16_t lTop_ = -1, lSel_ = -1;
    int32_t lTrack_ = -2;
    uint32_t lKey_ = 0;             // settings values hash -> redraw on change

    // now playing cache
    uint32_t npSeq_ = 0;
    int32_t npPos_ = -1, npDur_ = -1, npBar_ = -1;
    int8_t npPaused_ = -1, npVol_ = -1;
    uint32_t npStatKey_ = 0;

    // settings (NVS "ui")
    uint8_t briIdx_ = 4, dimIdx_ = 1, offIdx_ = 1, sleepIdx_ = 0;
    uint32_t sleepAt_ = 0;
    uint16_t batMv_ = 0;
    uint8_t batPct_ = 0;

    Panel scr_ = Panel::On;
    uint32_t lastTouch_ = 0;
};
