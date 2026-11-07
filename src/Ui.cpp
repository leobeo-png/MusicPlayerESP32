#include "Ui.h"
#include "Config.h"
#include <SD.h>
#include <Preferences.h>

// Optional: font compiled into flash (tools/vlw_to_header.py). Removes all font reads from the
// SD card, which otherwise share the SPI bus with the MP3 stream on every glyph drawn.
#if __has_include("font_jp.h")
#include "font_jp.h"
#define UI_FLASH_FONT 1
#endif

namespace {
// ---- palette (RGB565), dark minimal ----
constexpr uint16_t C_BG     = 0x0000;  // black
constexpr uint16_t C_HDR    = 0x10A2;  // #101410 header bar
constexpr uint16_t C_TEXT   = 0xFFFF;
constexpr uint16_t C_SUB    = 0x9CF3;  // light grey
constexpr uint16_t C_DIM    = 0x4208;  // dark grey
constexpr uint16_t C_TRACK  = 0x2945;  // bar background
constexpr uint16_t C_ACCENT = 0x0656;  // teal

// ---- layout ----
constexpr int HDR_H = 30;
constexpr int ROW_H = 30, LIST_Y = HDR_H + 4, ROWS = 9;
constexpr int M = 16;  // side margin on Now Playing
constexpr int ALBUM_Y = 42, TITLE_Y = 66, TITLE_H = 42, ARTIST_Y = 110;
constexpr int BAR_Y = 150, BAR_H = 6, TIME_Y = 162;
constexpr int TRANS_CY = 222, VOL_Y = 274, STAT_Y = 294;
constexpr uint8_t BL_CH = 0;

// ---- settings tables ----
constexpr uint8_t  BRI_PCT[]  = {20, 40, 60, 80, 100};
constexpr uint16_t DIM_S[]    = {5, 10, 30, 0};
constexpr uint16_t OFF_S[]    = {15, 30, 60, 0};
constexpr uint8_t  SLEEP_MIN[] = {0, 15, 30, 60, 90};
template <typename T, size_t N> constexpr size_t countOf(const T (&)[N]) { return N; }

enum MainItem { M_NOW, M_SONGS, M_FOLDERS, M_SHUFFLE_ALL, M_SETTINGS, M_COUNT };
const char* const MAIN_LABELS[M_COUNT] = {"Now Playing", "Songs", "Folders", "Shuffle All", "Settings"};

enum SetItem { S_SHUFFLE, S_REPEAT, S_BRIGHT, S_DIM, S_OFF, S_SLEEP, S_RESCAN, S_BATTERY, S_LIBRARY, S_COUNT };
const char* const SET_LABELS[S_COUNT] = {"Shuffle", "Repeat", "Brightness", "Dim after", "Screen off",
                                         "Sleep timer", "Rescan library", "Battery", "Library"};

void fmtSecs(char* b, size_t n, uint16_t s) {
    if (!s) snprintf(b, n, "Never");
    else if (s < 60) snprintf(b, n, "%u s", s);
    else snprintf(b, n, "%u min", s / 60);
}
} // namespace

// ================================================================== setup

void Ui::begin() {
    tft_.init();
    tft_.setRotation(2);
    tft_.invertDisplay(false);
    tft_.setSwapBytes(true);
    W_ = tft_.width();
    H_ = tft_.height();
    tft_.fillScreen(C_BG);
#ifdef TFT_BL
    ledcSetup(BL_CH, 5000, 8);
    ledcAttachPin(TFT_BL, BL_CH);
#endif
    loadSettings();
    setBacklight(BRI_PCT[briIdx_] * 255 / 100);
    lastTouch_ = millis();
}

void Ui::loadFont() {
#ifdef UI_FLASH_FONT
    tft_.loadFont(font_jp);
    Serial.println("[ui] font: flash");
#else
    if (SD.exists(String(cfg::SD_FONT) + ".vlw")) {
        tft_.loadFont(cfg::SD_FONT, SD);
        Serial.println("[ui] font: SD (convert to flash for speed, see tools/)");
    } else {
        tft_.setTextSize(2);  // built-in font fallback (no Japanese)
    }
#endif
}

void Ui::loadSettings() {
    Preferences p;
    p.begin("ui", true);
    briIdx_ = min<uint8_t>(p.getUChar("bri", countOf(BRI_PCT) - 1), countOf(BRI_PCT) - 1);
    dimIdx_ = min<uint8_t>(p.getUChar("dim", 1), countOf(DIM_S) - 1);
    offIdx_ = min<uint8_t>(p.getUChar("off", 1), countOf(OFF_S) - 1);
    p.end();
}

void Ui::saveSettings() {
    Preferences p;
    p.begin("ui", false);
    p.putUChar("bri", briIdx_);
    p.putUChar("dim", dimIdx_);
    p.putUChar("off", offIdx_);
    p.end();
}

bool Ui::takeRescanFlag() {
    Preferences p;
    p.begin("ui", false);
    bool f = p.getBool("rescan", false);
    if (f) p.remove("rescan");
    p.end();
    return f;
}

void Ui::message(const char* l1, const char* l2) {
    tft_.fillScreen(C_BG);
    tft_.setTextDatum(MC_DATUM);
    tft_.setTextColor(C_TEXT, C_BG);
    tft_.drawString(l1, W_ / 2, H_ / 2 - (l2 ? 12 : 0));
    if (l2) {
        tft_.setTextColor(C_SUB, C_BG);
        tft_.drawString(l2, W_ / 2, H_ / 2 + 14);
    }
    tft_.setTextDatum(TL_DATUM);
    full_ = true;
    hdrTitle_ = nullptr;
}

// ================================================================== navigation

void Ui::push(Screen s, int16_t arg, int16_t sel) {
    if (depth_ >= countOf(stack_)) depth_ = 1;  // keep Main, drop the rest
    stack_[depth_++] = View{s, sel, 0, arg};
    full_ = true;
}

void Ui::pop() {
    if (depth_ > 1) { --depth_; full_ = true; }
}

void Ui::goNowPlaying() {
    if (cur().scr != Screen::NowPlaying) push(Screen::NowPlaying);
}

void Ui::showHome() {
    depth_ = 0;
    push(Screen::Main);
    push(Screen::NowPlaying);
}

// ================================================================== list model

const char* Ui::title(const View& v) const {
    switch (v.scr) {
    case Screen::NowPlaying:  return "Now Playing";
    case Screen::Main:        return "Music";
    case Screen::Songs:       return "Songs";
    case Screen::Folders:     return "Folders";
    case Screen::FolderSongs: return "Folder";
    case Screen::Settings:    return "Settings";
    }
    return "";
}

int Ui::itemCount(const View& v) const {
    switch (v.scr) {
    case Screen::Main:        return M_COUNT;
    case Screen::Songs:       return (int)lib_.count();
    case Screen::Folders:     return (int)lib_.folderCount();
    case Screen::FolderSongs: return v.arg < (int)lib_.folderCount() ? lib_.folder(v.arg).count : 0;
    case Screen::Settings:    return S_COUNT;
    default:                  return 0;
    }
}

int Ui::itemSong(const View& v, int i) const {
    if (v.scr == Screen::Songs) return lib_.az(i);
    if (v.scr == Screen::FolderSongs) return lib_.folder(v.arg).first + i;
    return -1;
}

void Ui::itemLabel(const View& v, int i, char* buf, size_t len) const {
    buf[0] = '\0';
    char path[256];
    switch (v.scr) {
    case Screen::Main:     strlcpy(buf, MAIN_LABELS[i], len); break;
    case Screen::Settings: strlcpy(buf, SET_LABELS[i], len); break;
    case Screen::Folders:  lib_.folderName(i, buf, len); break;
    case Screen::Songs:
    case Screen::FolderSongs:
        if (lib_.path(itemSong(v, i), path, sizeof path)) MusicLibrary::displayName(path, buf, len);
        break;
    default: break;
    }
}

bool Ui::itemHasChild(const View& v, int i) const {
    if (v.scr == Screen::Main) return i == M_SONGS || i == M_FOLDERS || i == M_SETTINGS;
    return v.scr == Screen::Folders;
}

bool Ui::itemValue(const View& v, int i, char* b, size_t n, const PlayerStatus& s) const {
    if (v.scr == Screen::Folders) { snprintf(b, n, "%u", lib_.folder(i).count); return true; }
    if (v.scr != Screen::Settings) return false;
    static const char* const REP[] = {"Off", "All", "One"};
    switch (i) {
    case S_SHUFFLE: snprintf(b, n, "%s", s.shuffle ? "On" : "Off"); return true;
    case S_REPEAT:  snprintf(b, n, "%s", REP[(int)s.repeat]); return true;
    case S_BRIGHT:  snprintf(b, n, "%u%%", BRI_PCT[briIdx_]); return true;
    case S_DIM:     fmtSecs(b, n, DIM_S[dimIdx_]); return true;
    case S_OFF:     fmtSecs(b, n, OFF_S[offIdx_]); return true;
    case S_SLEEP:
        if (sleepAt_) snprintf(b, n, "%lu min left", (unsigned long)((sleepAt_ - millis()) / 60000 + 1));
        else snprintf(b, n, "Off");
        return true;
    case S_BATTERY: snprintf(b, n, "%u.%02u V  %u%%", batMv_ / 1000, (batMv_ % 1000) / 10, batPct_); return true;
    case S_LIBRARY: snprintf(b, n, "%u songs", (unsigned)lib_.count()); return true;
    default: return false;
    }
}

void Ui::select(int i) {
    const View v = cur();
    const PlayerStatus s = player_.status();
    switch (v.scr) {
    case Screen::Main:
        switch (i) {
        case M_NOW:     goNowPlaying(); break;
        case M_SONGS:   push(Screen::Songs, 0, max(0, lib_.azPos(s.track))); break;
        case M_FOLDERS: push(Screen::Folders, 0, max(0, lib_.folderOf(s.track))); break;
        case M_SHUFFLE_ALL:
            if (lib_.count()) {
                player_.send(PlayerCmd::SetShuffle, 1);
                player_.send(PlayerCmd::PlayQueue, (int)QueueKind::AllAZ, 0, lib_.az(esp_random() % lib_.count()));
                goNowPlaying();
            }
            break;
        case M_SETTINGS: push(Screen::Settings); break;
        }
        break;

    case Screen::Songs:
        player_.send(PlayerCmd::PlayQueue, (int)QueueKind::AllAZ, 0, lib_.az(i));
        goNowPlaying();
        break;

    case Screen::Folders: {
        const Folder& f = lib_.folder(i);
        int sel = (s.track >= f.first && s.track < f.first + f.count) ? s.track - f.first : 0;
        push(Screen::FolderSongs, i, sel);
        break;
    }

    case Screen::FolderSongs:
        player_.send(PlayerCmd::PlayQueue, (int)QueueKind::Folder, v.arg, lib_.folder(v.arg).first + i);
        goNowPlaying();
        break;

    case Screen::Settings:
        switch (i) {
        case S_SHUFFLE: player_.send(PlayerCmd::SetShuffle, !s.shuffle); break;
        case S_REPEAT:  player_.send(PlayerCmd::SetRepeat, ((int)s.repeat + 1) % 3); break;
        case S_BRIGHT:
            briIdx_ = (briIdx_ + 1) % countOf(BRI_PCT);
            setBacklight(BRI_PCT[briIdx_] * 255 / 100);
            saveSettings();
            break;
        case S_DIM: dimIdx_ = (dimIdx_ + 1) % countOf(DIM_S); saveSettings(); break;
        case S_OFF: offIdx_ = (offIdx_ + 1) % countOf(OFF_S); saveSettings(); break;
        case S_SLEEP:
            sleepIdx_ = (sleepIdx_ + 1) % countOf(SLEEP_MIN);
            sleepAt_ = SLEEP_MIN[sleepIdx_] ? (millis() + SLEEP_MIN[sleepIdx_] * 60000UL) | 1 : 0;
            break;
        case S_RESCAN: {
            message("Rescanning library...");
            player_.send(PlayerCmd::Pause);  // saves resume position
            delay(300);
            Preferences p;
            p.begin("ui", false);
            p.putBool("rescan", true);
            p.end();
            ESP.restart();
            break;
        }
        default: break;
        }
        break;

    default: break;
    }
}

// ================================================================== input

void Ui::handle(const InputMsg& m) {
    wake();
    View& v = cur();

    if (v.scr == Screen::NowPlaying) {
        switch (m.ev) {
        case InputEvent::Rotate:     player_.send(PlayerCmd::VolumeDelta, m.value); break;
        case InputEvent::RotateHeld:
            for (int i = 0; i < abs(m.value); ++i)
                player_.send(m.value > 0 ? PlayerCmd::Next : PlayerCmd::Prev);
            break;
        case InputEvent::Click:      player_.send(PlayerCmd::TogglePause); break;
        case InputEvent::LongPress:  pop(); break;
        default: break;
        }
        return;
    }

    const int n = itemCount(v);
    switch (m.ev) {
    case InputEvent::Rotate:     v.sel = constrain(v.sel + m.value, 0, max(0, n - 1)); break;
    case InputEvent::RotateHeld: v.sel = constrain(v.sel + m.value * ROWS, 0, max(0, n - 1)); break;
    case InputEvent::Click:      if (n) select(v.sel); break;
    case InputEvent::LongPress:  if (depth_ > 1) pop(); else goNowPlaying(); break;
    default: break;
    }
}

// ================================================================== rendering

void Ui::clearBody() { tft_.fillRect(0, HDR_H, W_, H_ - HDR_H, C_BG); }

void Ui::render(const PlayerStatus& s, uint8_t batPct, uint16_t batMv) {
    batMv_ = batMv;
    batPct_ = batPct;
    if (scr_ == Panel::Off) return;  // panel asleep: no SPI traffic

    if (full_) {
        clearBody();
        hdrTitle_ = nullptr;
        lTop_ = lSel_ = -1;
        npSeq_ = s.metaSeq - 1;
        npPos_ = npDur_ = npBar_ = -1;
        npPaused_ = npVol_ = -1;
        npStatKey_ = UINT32_MAX;
        full_ = false;
    }

    const View& v = cur();
    const char* t = title(v);
    if (t != hdrTitle_ || (int8_t)s.paused != hdrPaused_ || (int8_t)batPct != hdrPct_) {
        hdrTitle_ = t;
        hdrPaused_ = s.paused;
        hdrPct_ = batPct;
        drawHeader(t, s.paused, batPct);
    }

    if (v.scr == Screen::NowPlaying) renderNowPlaying(s);
    else renderList(s);
}

void Ui::drawHeader(const char* t, bool paused, uint8_t pct) {
    tft_.fillRect(0, 0, W_, HDR_H, C_HDR);
    tft_.setTextColor(C_TEXT, C_HDR);
    tft_.drawString(t, 12, 7);

    // play state glyph
    const int gx = W_ - 52, gy = 9;
    if (paused) {
        tft_.fillRect(gx, gy, 3, 12, C_SUB);
        tft_.fillRect(gx + 6, gy, 3, 12, C_SUB);
    } else {
        tft_.fillTriangle(gx, gy, gx, gy + 12, gx + 9, gy + 6, C_SUB);
    }

    // battery
    const int bx = W_ - 34, by = 9;
    tft_.drawRoundRect(bx, by, 22, 12, 2, C_SUB);
    tft_.fillRect(bx + 22, by + 4, 2, 4, C_SUB);
    const uint16_t c = pct > 30 ? C_ACCENT : pct > 15 ? TFT_YELLOW : TFT_RED;
    const int fw = 18 * pct / 100;
    if (fw > 0) tft_.fillRect(bx + 2, by + 2, fw, 8, c);
}

// ------------------------------------------------------------------ lists

void Ui::renderList(const PlayerStatus& s) {
    View& v = cur();
    const int n = itemCount(v);
    v.sel = constrain((int)v.sel, 0, max(0, n - 1));
    if (v.sel < v.top) v.top = v.sel;
    else if (v.sel >= v.top + ROWS) v.top = v.sel - ROWS + 1;
    v.top = constrain((int)v.top, 0, max(0, n - ROWS));

    // Values that change underneath a list without the cursor moving.
    uint32_t key = 0;
    if (v.scr == Screen::Settings) {
        uint32_t sleepMin = sleepAt_ ? (sleepAt_ - millis()) / 60000 : 0;
        key = (uint32_t)s.shuffle | (uint32_t)s.repeat << 1 | (uint32_t)briIdx_ << 3 | (uint32_t)dimIdx_ << 6 |
              (uint32_t)offIdx_ << 8 | (sleepMin & 0x7F) << 10 | (uint32_t)(batMv_ / 10) << 17;
    }
    const bool songList = v.scr == Screen::Songs || v.scr == Screen::FolderSongs;
    const bool all = lTop_ != v.top || key != lKey_ || (songList && s.track != lTrack_);

    if (all) {
        for (int r = 0; r < ROWS; ++r) drawRow(v, r, v.top + r, s);
        drawScrollbar(n, v.top);
        if (n == 0) {
            tft_.setTextColor(C_SUB, C_BG);
            tft_.drawString("(empty)", 16, LIST_Y + 8);
        }
    } else if (v.sel != lSel_) {
        if (lSel_ >= v.top && lSel_ < v.top + ROWS) drawRow(v, lSel_ - v.top, lSel_, s);
        drawRow(v, v.sel - v.top, v.sel, s);
    }
    lTop_ = v.top;
    lSel_ = v.sel;
    lKey_ = key;
    lTrack_ = s.track;
}

void Ui::drawRow(const View& v, int row, int i, const PlayerStatus& s) {
    const int y = LIST_Y + row * ROW_H;
    const int rowW = W_ - 10;  // leave room for the scrollbar
    tft_.fillRect(0, y, rowW, ROW_H, C_BG);
    if (i >= itemCount(v)) return;

    const bool sel = i == v.sel;
    uint16_t bg = C_BG, fg = C_TEXT, vfg = C_SUB;
    if (sel) {
        tft_.fillRoundRect(4, y + 1, rowW - 6, ROW_H - 2, 6, C_ACCENT);
        bg = C_ACCENT;
        fg = vfg = C_BG;
    } else if (itemSong(v, i) == s.track && s.track >= 0) {
        fg = C_ACCENT;  // currently playing song
    }

    int right = rowW - 10;
    if (itemHasChild(v, i)) {  // chevron
        const int cx = right - 4, cy = y + ROW_H / 2;
        tft_.drawLine(cx - 3, cy - 5, cx + 2, cy, vfg);
        tft_.drawLine(cx - 3, cy + 5, cx + 2, cy, vfg);
        tft_.drawLine(cx - 4, cy - 5, cx + 1, cy, vfg);
        tft_.drawLine(cx - 4, cy + 5, cx + 1, cy, vfg);
        right -= 14;
    }
    char val[32];
    if (itemValue(v, i, val, sizeof val, s)) {
        const int w = tft_.textWidth(val);
        tft_.setTextColor(vfg, bg);
        tft_.drawString(val, right - w, y + 7);
        right -= w + 10;
    }

    char label[128];
    itemLabel(v, i, label, sizeof label);
    tft_.setViewport(14, y, max(10, right - 14), ROW_H);
    tft_.setTextWrap(false);
    tft_.setTextColor(fg, bg);
    tft_.setCursor(0, 7);
    tft_.print(label);
    tft_.resetViewport();
}

void Ui::drawScrollbar(int n, int top) {
    const int x = W_ - 5, h = ROWS * ROW_H;
    tft_.fillRect(x - 1, LIST_Y, 5, h, C_BG);
    if (n <= ROWS) return;
    tft_.fillRect(x, LIST_Y, 3, h, C_TRACK);
    const int th = max(14, h * ROWS / n);
    const int ty = LIST_Y + (h - th) * top / (n - ROWS);
    tft_.fillRect(x, ty, 3, th, C_SUB);
}

// ------------------------------------------------------------------ now playing

void Ui::renderNowPlaying(const PlayerStatus& s) {
    if (s.metaSeq != npSeq_) { npSeq_ = s.metaSeq; npMeta(s); }

    const bool timeChanged = (int32_t)s.posSec != npPos_ || (int32_t)s.durSec != npDur_;
    if (timeChanged || npBar_ < 0) npProgress(s, npBar_ < 0);

    if ((int8_t)s.paused != npPaused_) { npPaused_ = s.paused; npTransport(s.paused); }
    if ((int8_t)s.volume != npVol_)    { npVol_ = s.volume; npVolume(s.volume); }

    const uint32_t key = (uint32_t)s.shuffle | (uint32_t)s.repeat << 1 | (uint32_t)s.qpos << 3 | (uint32_t)s.qlen << 16;
    if (key != npStatKey_) { npStatKey_ = key; npStatus(s); }
}

void Ui::npMeta(const PlayerStatus& s) {
    tft_.fillRect(0, ALBUM_Y - 2, W_, ARTIST_Y + 22 - ALBUM_Y, C_BG);
    tft_.setTextWrap(false);

    char album[96] = "";
    int f = s.track >= 0 ? lib_.folderOf(s.track) : -1;
    if (f >= 0) lib_.folderName(f, album, sizeof album);
    tft_.setViewport(M, ALBUM_Y, W_ - 2 * M, 20);
    tft_.setTextColor(C_SUB, C_BG);
    tft_.setCursor(0, 0);
    tft_.print(album);

    tft_.setViewport(M, TITLE_Y, W_ - 2 * M, TITLE_H);  // 2 lines max, clipped
    tft_.setTextWrap(true);
    tft_.setTextColor(C_TEXT, C_BG);
    tft_.setCursor(0, 0);
    tft_.print(s.title);

    tft_.setViewport(M, ARTIST_Y, W_ - 2 * M, 20);
    tft_.setTextWrap(false);
    tft_.setTextColor(C_ACCENT, C_BG);
    tft_.setCursor(0, 0);
    tft_.print(s.artist);
    tft_.resetViewport();
}

void Ui::npProgress(const PlayerStatus& s, bool full) {
    const int bw = W_ - 2 * M;
    const int w = s.durSec ? (int)((uint64_t)bw * min(s.posSec, s.durSec) / s.durSec) : 0;
    if (full) {
        tft_.fillRect(M, BAR_Y, bw, BAR_H, C_TRACK);
        if (w) tft_.fillRect(M, BAR_Y, w, BAR_H, C_ACCENT);
    } else if (w > npBar_) {
        tft_.fillRect(M + npBar_, BAR_Y, w - npBar_, BAR_H, C_ACCENT);
    } else if (w < npBar_) {
        tft_.fillRect(M + w, BAR_Y, npBar_ - w, BAR_H, C_TRACK);
    }
    npBar_ = w;

    if ((int32_t)s.posSec == npPos_ && (int32_t)s.durSec == npDur_) return;
    npPos_ = s.posSec;
    npDur_ = s.durSec;
    char a[16], b[16];
    const uint32_t rem = s.durSec > s.posSec ? s.durSec - s.posSec : 0;
    snprintf(a, sizeof a, "%u:%02u", (unsigned)(s.posSec / 60), (unsigned)(s.posSec % 60));
    snprintf(b, sizeof b, "-%u:%02u", (unsigned)(rem / 60), (unsigned)(rem % 60));
    tft_.fillRect(M, TIME_Y, bw, 20, C_BG);
    tft_.setTextColor(C_SUB, C_BG);
    tft_.drawString(a, M, TIME_Y);
    tft_.setTextDatum(TR_DATUM);
    tft_.drawString(b, W_ - M, TIME_Y);
    tft_.setTextDatum(TL_DATUM);
}

void Ui::npTransport(bool paused) {
    const int cx = W_ / 2, cy = TRANS_CY;
    tft_.fillRect(M, cy - 30, W_ - 2 * M, 61, C_BG);

    // centre button shows the action a click performs
    tft_.fillCircle(cx, cy, 27, C_ACCENT);
    if (paused) {
        tft_.fillTriangle(cx - 7, cy - 12, cx - 7, cy + 12, cx + 13, cy, C_BG);
    } else {
        tft_.fillRect(cx - 9, cy - 11, 6, 22, C_BG);
        tft_.fillRect(cx + 3, cy - 11, 6, 22, C_BG);
    }

    // prev / next (hold + rotate)
    const int px = cx - 72, nx = cx + 72;
    tft_.fillTriangle(px + 1, cy, px + 11, cy - 8, px + 11, cy + 8, C_SUB);
    tft_.fillTriangle(px - 9, cy, px + 1, cy - 8, px + 1, cy + 8, C_SUB);
    tft_.fillRect(px - 12, cy - 8, 3, 17, C_SUB);
    tft_.fillTriangle(nx - 1, cy, nx - 11, cy - 8, nx - 11, cy + 8, C_SUB);
    tft_.fillTriangle(nx + 9, cy, nx - 1, cy - 8, nx - 1, cy + 8, C_SUB);
    tft_.fillRect(nx + 10, cy - 8, 3, 17, C_SUB);
}

void Ui::npVolume(uint8_t v) {
    const int x0 = M + 22, bw = W_ - M - x0;
    tft_.fillRect(M, VOL_Y - 8, W_ - 2 * M, 17, C_BG);
    // speaker icon
    tft_.fillRect(M, VOL_Y - 3, 5, 7, C_SUB);
    tft_.fillTriangle(M + 3, VOL_Y, M + 11, VOL_Y - 7, M + 11, VOL_Y + 7, C_SUB);
    // bar
    tft_.fillRect(x0, VOL_Y - 2, bw, 4, C_TRACK);
    const int w = bw * v / cfg::VOLUME_MAX;
    if (w) tft_.fillRect(x0, VOL_Y - 2, w, 4, C_ACCENT);
}

void Ui::npStatus(const PlayerStatus& s) {
    tft_.fillRect(0, STAT_Y - 2, W_, H_ - STAT_Y + 2, C_BG);
    tft_.setTextColor(s.shuffle ? C_ACCENT : C_DIM, C_BG);
    tft_.drawString("Shuffle", M, STAT_Y);
    const char* rep = s.repeat == Repeat::One ? "Repeat 1" : "Repeat";
    tft_.setTextColor(s.repeat != Repeat::Off ? C_ACCENT : C_DIM, C_BG);
    tft_.drawString(rep, M + tft_.textWidth("Shuffle") + 14, STAT_Y);

    char q[16];
    snprintf(q, sizeof q, "%u / %u", s.qlen ? s.qpos + 1 : 0, s.qlen);
    tft_.setTextColor(C_SUB, C_BG);
    tft_.setTextDatum(TR_DATUM);
    tft_.drawString(q, W_ - M, STAT_Y);
    tft_.setTextDatum(TL_DATUM);
}

// ================================================================== power

void Ui::setBacklight(uint8_t level) {
#ifdef TFT_BL
  #if defined(TFT_BACKLIGHT_ON) && (TFT_BACKLIGHT_ON == LOW)
    level = 255 - level;
  #endif
    ledcWrite(BL_CH, level);
#endif
}

uint32_t Ui::dimMs() const { return DIM_S[dimIdx_] * 1000UL; }
uint32_t Ui::offMs() const { return OFF_S[offIdx_] * 1000UL; }

void Ui::wake() {
    lastTouch_ = millis();
    if (scr_ == Panel::Off) {
        tft_.writecommand(TFT_SLPOUT);
        delay(10);    // ST7789 needs 5 ms after SLPOUT; audio runs in its own task
        full_ = true; // draws were skipped while off
    }
    if (scr_ != Panel::On) {
        setBacklight(BRI_PCT[briIdx_] * 255 / 100);
        scr_ = Panel::On;
    }
}

void Ui::sleepPanel() {
    setBacklight(0);
    tft_.writecommand(TFT_SLPIN);  // controller sleep: ~mA -> ~µA
    scr_ = Panel::Off;
}

void Ui::updatePower() {
    const uint32_t idle = millis() - lastTouch_;
    const uint32_t off = offMs(), dim = dimMs();
    if (scr_ != Panel::Off && off && idle >= off) {
        sleepPanel();
        showHome();  // blind knob use while dark = volume / pause on Now Playing
    } else if (scr_ == Panel::On && dim && idle >= dim) {
        setBacklight(min<uint32_t>(cfg::BRIGHTNESS_DIM, BRI_PCT[briIdx_] * 255 / 100));
        scr_ = Panel::Dim;
    }
}
