#include "Ui.h"
#include "Config.h"
#include <SD.h>

// Optional: font compiled into flash (tools/vlw_to_header.py). Removes all font reads from the
// SD card, which otherwise share the SPI bus with the MP3 stream on every glyph drawn.
#if __has_include("font_jp.h")
#include "font_jp.h"
#define UI_FLASH_FONT 1
#endif

namespace {
constexpr int ROWS = 4, ROW_H = 20, LIST_Y = 240;
constexpr int TITLE_Y = 28, TITLE_H = 44, ARTIST_Y = 73, ARTIST_H = 22;
constexpr int TIME_Y = 100, BAR_Y = 130, BAR_H = 10, VOL_Y = 150;
constexpr int MARGIN = 10;
constexpr uint8_t BL_CH = 0;
constexpr uint16_t C_BG = TFT_BLACK, C_TEXT = TFT_WHITE, C_TITLE = TFT_YELLOW,
                   C_ACCENT = TFT_CYAN, C_BROWSE = TFT_ORANGE, C_DIM = TFT_DARKGREY;
}

void Ui::begin() {
    tft_.init();
    tft_.setRotation(2);
    tft_.setSwapBytes(true);
    W_ = tft_.width();
    H_ = tft_.height();
    tft_.fillScreen(C_BG);

#ifdef TFT_BL
    ledcSetup(BL_CH, 5000, 8);
    ledcAttachPin(TFT_BL, BL_CH);
#endif
    setBacklight(cfg::BRIGHTNESS_FULL);
    lastTouch_ = millis();
}

void Ui::loadFont() {
#ifdef UI_FLASH_FONT
    tft_.loadFont(font_jp);
    Serial.println("[ui] font: flash");
#else
    // Needs SD mounted first. Works, but every glyph is an SD read on the shared bus.
    if (SD.exists(String(cfg::SD_FONT) + ".vlw")) {
        tft_.loadFont(cfg::SD_FONT, SD);
        Serial.println("[ui] font: SD (convert to flash for speed, see tools/)");
    }
#endif
}

void Ui::clear() {
    tft_.fillScreen(C_BG);
    invalidate();
}

void Ui::setBacklight(uint8_t level) {
#ifdef TFT_BL
  #if defined(TFT_BACKLIGHT_ON) && (TFT_BACKLIGHT_ON == LOW)
    level = 255 - level;
  #endif
    ledcWrite(BL_CH, level);
#endif
}

void Ui::message(const char* l1, const char* l2) {
    tft_.fillScreen(C_BG);
    tft_.setTextColor(C_TEXT, C_BG);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString(l1, W_ / 2, H_ / 2 - (l2 ? 12 : 0));
    if (l2) {
        tft_.setTextColor(C_DIM, C_BG);
        tft_.drawString(l2, W_ / 2, H_ / 2 + 12);
    }
    tft_.setTextDatum(TL_DATUM);
    invalidate();
}

void Ui::invalidate() {
    metaSeq_ = UINT32_MAX;
    track_ = listHi_ = -2;
    listTop_ = posSec_ = durSec_ = barW_ = -1;
    paused_ = vol_ = bat_ = -1;
    volVisible_ = false;
}

// ---------------------------------------------------------------- drawing

void Ui::drawHeader(const PlayerStatus& s) {
    char buf[24];
    snprintf(buf, sizeof buf, "%ld / %u", (long)s.track + 1, (unsigned)lib_.count());
    tft_.fillRect(0, 4, W_ - 70, 22, C_BG);
    tft_.setTextColor(C_DIM, C_BG);
    tft_.drawString(buf, MARGIN, 8);
}

void Ui::drawMeta(const PlayerStatus& s) {
    tft_.fillRect(0, TITLE_Y, W_, TITLE_H + ARTIST_H + 1, C_BG);

    // Viewports clip long titles instead of letting them spill into the next block.
    tft_.setViewport(MARGIN, TITLE_Y, W_ - 2 * MARGIN, TITLE_H);
    tft_.setTextWrap(true);
    tft_.setTextColor(C_TITLE, C_BG);
    tft_.setCursor(0, 2);
    tft_.print(s.title);

    tft_.setViewport(MARGIN, ARTIST_Y, W_ - 2 * MARGIN, ARTIST_H);
    tft_.setTextWrap(false);
    tft_.setTextColor(C_TEXT, C_BG);
    tft_.setCursor(0, 2);
    tft_.print(s.artist);
    tft_.resetViewport();
}

void Ui::drawTime(const PlayerStatus& s) {
    char buf[32];
    snprintf(buf, sizeof buf, "%02u:%02u / %02u:%02u",
             (unsigned)(s.posSec / 60), (unsigned)(s.posSec % 60),
             (unsigned)(s.durSec / 60), (unsigned)(s.durSec % 60));
    tft_.fillRect(0, TIME_Y - 2, W_ - 40, 22, C_BG);
    tft_.setTextColor(C_TEXT, C_BG);
    tft_.drawString(buf, MARGIN, TIME_Y);
}

void Ui::drawProgress(const PlayerStatus& s) {
    const int full = W_ - 2 * MARGIN - 2;  // inside the outline
    int w = s.durSec ? (int)((uint64_t)full * min(s.posSec, s.durSec) / s.durSec) : 0;
    if (barW_ < 0) {
        tft_.fillRect(MARGIN, BAR_Y, W_ - 2 * MARGIN, BAR_H, C_BG);
        tft_.drawRect(MARGIN, BAR_Y, W_ - 2 * MARGIN, BAR_H, C_TEXT);
        barW_ = 0;
    }
    if (w == barW_) return;
    if (w > barW_) tft_.fillRect(MARGIN + 1 + barW_, BAR_Y + 1, w - barW_, BAR_H - 2, C_TEXT);
    else           tft_.fillRect(MARGIN + 1 + w,     BAR_Y + 1, barW_ - w, BAR_H - 2, C_BG);
    barW_ = w;
}

void Ui::drawPlayIcon(bool paused) {
    const int x = W_ - 30, y = 92;
    tft_.fillRect(x, y, 26, 26, C_BG);
    if (paused) {
        tft_.fillRect(x + 5, y + 3, 5, 20, C_TEXT);
        tft_.fillRect(x + 15, y + 3, 5, 20, C_TEXT);
    } else {
        tft_.fillTriangle(x + 5, y + 3, x + 5, y + 23, x + 21, y + 13, C_TEXT);
    }
}

void Ui::drawVolume(uint8_t v, bool visible) {
    const int w = 100, x = W_ - MARGIN - w;
    tft_.fillRect(x - 40, VOL_Y - 4, w + 40, 18, C_BG);
    if (!visible) return;
    char buf[8];
    snprintf(buf, sizeof buf, "%u", v);
    tft_.setTextColor(C_ACCENT, C_BG);
    tft_.setTextDatum(TR_DATUM);
    tft_.drawString(buf, x - 6, VOL_Y - 3);
    tft_.setTextDatum(TL_DATUM);
    tft_.drawRect(x, VOL_Y, w, 10, C_TEXT);
    tft_.fillRect(x + 1, VOL_Y + 1, (w - 2) * v / cfg::VOLUME_MAX, 8, C_ACCENT);
}

void Ui::drawBattery(uint8_t pct) {
    const int x = W_ - 30, y = 10;
    tft_.fillRect(x - 1, y - 2, 28, 16, C_BG);
    tft_.drawRect(x, y, 20, 12, C_TEXT);
    tft_.fillRect(x + 20, y + 4, 3, 4, C_TEXT);
    uint16_t c = pct > 30 ? TFT_GREEN : pct > 15 ? TFT_YELLOW : TFT_RED;
    int fw = 18 * pct / 100;
    if (fw > 0) tft_.fillRect(x + 1, y + 1, fw, 10, c);
}

void Ui::drawList(int32_t hi) {
    const int32_t n = (int32_t)lib_.count();
    if (n == 0) return;
    int32_t top = listTop_ < 0 ? 0 : listTop_;
    if (hi < top) top = hi;
    else if (hi >= top + ROWS) top = hi - ROWS + 1;
    top = constrain(top, 0, max<int32_t>(0, n - ROWS));

    char path[256], name[128];
    for (int r = 0; r < ROWS; ++r) {
        const int32_t i = top + r;
        const int y = LIST_Y + r * ROW_H;
        uint16_t bg = C_BG, fg = C_TEXT;
        if (browsing_ && i == cursor_)        { bg = C_BROWSE; fg = TFT_BLACK; }
        else if (!browsing_ && i == track_)   { bg = C_ACCENT; fg = TFT_BLACK; }
        else if (browsing_ && i == track_)    { fg = C_ACCENT; }
        tft_.fillRect(0, y, W_, ROW_H, bg);
        if (i >= n || !lib_.path(i, path, sizeof path)) continue;
        MusicLibrary::displayName(path, name, sizeof name);
        tft_.setViewport(MARGIN, y, W_ - 2 * MARGIN, ROW_H);
        tft_.setTextWrap(false);
        tft_.setTextColor(fg, bg);
        tft_.setCursor(0, 2);
        tft_.print(name);
        tft_.resetViewport();
    }
    listTop_ = top;
}

void Ui::render(const PlayerStatus& s, uint8_t batteryPct) {
    if (state_ == Scr::Off) return;  // panel asleep: skip all SPI traffic

    const bool trackChanged = s.track != track_;
    if (s.metaSeq != metaSeq_) {
        metaSeq_ = s.metaSeq;
        drawMeta(s);
        drawHeader(s);
    }
    if ((int32_t)s.posSec != posSec_ || (int32_t)s.durSec != durSec_) {
        posSec_ = s.posSec;
        durSec_ = s.durSec;
        drawTime(s);
    }
    drawProgress(s);
    if ((int8_t)s.paused != paused_) { paused_ = s.paused; drawPlayIcon(s.paused); }
    if ((int8_t)batteryPct != bat_) { bat_ = batteryPct; drawBattery(batteryPct); }

    // Volume overlay: visible for 2 s after a change.
    const bool wantVol = volShownAt_ && millis() - volShownAt_ < 2000;
    if (wantVol != volVisible_ || (wantVol && s.volume != vol_)) {
        volVisible_ = wantVol;
        vol_ = s.volume;
        drawVolume(s.volume, wantVol);
    }

    const int32_t hi = browsing_ ? cursor_ : s.track;
    if (trackChanged || hi != listHi_) {
        track_ = s.track;
        listHi_ = hi;
        drawList(hi);
    }
}

// ---------------------------------------------------------------- browse mode

void Ui::browseStart(int32_t from) {
    browsing_ = true;
    cursor_ = max<int32_t>(0, from);
    listHi_ = -2;  // force list redraw with browse colours
}

void Ui::browseMove(int delta) {
    const int32_t n = (int32_t)lib_.count();
    if (!n) return;
    cursor_ = constrain(cursor_ + delta, 0, n - 1);
}

int32_t Ui::browseEnd() {
    browsing_ = false;
    listHi_ = -2;
    return cursor_;
}

// ---------------------------------------------------------------- power

void Ui::touch() {
    lastTouch_ = millis();
    if (state_ == Scr::Off) {
        tft_.writecommand(TFT_SLPOUT);
        delay(10);         // ST7789 needs 5 ms after SLPOUT; audio runs in its own task
        invalidate();      // draws were skipped while off
    }
    if (state_ != Scr::On) {
        setBacklight(cfg::BRIGHTNESS_FULL);
        state_ = Scr::On;
    }
}

void Ui::sleepPanel() {
    setBacklight(0);
    tft_.writecommand(TFT_SLPIN);
    state_ = Scr::Off;
}

void Ui::updatePower() {
    const uint32_t idle = millis() - lastTouch_;
    if (browsing_ && idle >= 8000) browseEnd();
    if (state_ != Scr::Off && idle >= cfg::SCREEN_OFF_MS) {
        sleepPanel();  // backlight off + controller sleep (~mA -> ~µA)
    } else if (state_ == Scr::On && idle >= cfg::DIM_AFTER_MS) {
        setBacklight(cfg::BRIGHTNESS_DIM);
        state_ = Scr::Dim;
    }
}
