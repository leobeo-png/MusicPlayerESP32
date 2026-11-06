#include "Player.h"
#include "Config.h"
#include <SD.h>
#include <Preferences.h>

static Player* g_player = nullptr;
static Preferences prefs;

// ---- Audio library callbacks (weak symbols in Audio.h); run in the audio task ----
void audio_id3data(const char* info) { if (g_player) g_player->onId3(info); }
void audio_eof_mp3(const char*)      { if (g_player) g_player->onEof(); }
// Left empty on purpose: the library calls this a lot and Serial TX blocks the audio task.
void audio_info(const char*) {}

void Player::begin() {
    g_player = this;
    q_ = xQueueCreate(16, sizeof(Msg));

    if (cfg::PIN_DAC_XSMT >= 0) { pinMode(cfg::PIN_DAC_XSMT, OUTPUT); digitalWrite(cfg::PIN_DAC_XSMT, HIGH); }
    audio_.setPinout(cfg::PIN_I2S_BCLK, cfg::PIN_I2S_LRC, cfg::PIN_I2S_DOUT);

    prefs.begin("player", false);
    volume_  = min<uint8_t>(prefs.getUChar("vol", cfg::VOLUME_DEFAULT), cfg::VOLUME_MAX);
    shuffle_ = prefs.getBool("shuf", false);
    repeat_  = (Repeat)min<uint8_t>(prefs.getUChar("rep", (uint8_t)Repeat::All), 2);
    qkind_   = (QueueKind)min<uint8_t>(prefs.getUChar("qkind", 0), 1);
    qarg_    = prefs.getInt("qarg", 0);
    audio_.setVolume(volume_);

    // Resume last song (by path, so a rescan that reorders files still finds it).
    String last = prefs.getString("path", "");
    uint32_t fpos = prefs.getULong("fpos", 0);
    int song = lib_.find(last.c_str());
    if (song < 0) { song = lib_.count() ? lib_.az(0) : 0; fpos = 0; qkind_ = QueueKind::AllAZ; }

    buildQueue(qkind_, qarg_, song);
    if (order_.empty() || order_[qpos_] != song) buildQueue(QueueKind::AllAZ, 0, song);

    portENTER_CRITICAL(&mux_);
    st_.volume = volume_;
    portEXIT_CRITICAL(&mux_);
    publishQueue();

    if (!order_.empty() && !play(order_[qpos_], fpos)) step(+1, false);

    xTaskCreatePinnedToCore(taskEntry, "audio", cfg::AUDIO_STACK, this, cfg::AUDIO_PRIO, nullptr, cfg::AUDIO_CORE);
}

void Player::send(PlayerCmd cmd, int32_t a, int32_t b, int32_t c) {
    Msg m{cmd, a, b, c};
    xQueueSend(q_, &m, 0);
}

PlayerStatus Player::status() {
    PlayerStatus s;
    portENTER_CRITICAL(&mux_);
    s = st_;
    portEXIT_CRITICAL(&mux_);
    return s;
}

void Player::onId3(const char* info) {
    portENTER_CRITICAL(&mux_);
    if (strncmp(info, "Title: ", 7) == 0) {
        strlcpy(st_.title, info + 7, sizeof st_.title);
        st_.metaSeq++;
    } else if (strncmp(info, "Artist: ", 8) == 0) {
        strlcpy(st_.artist, info + 8, sizeof st_.artist);
        st_.metaSeq++;
    }
    portEXIT_CRITICAL(&mux_);
}

// ------------------------------------------------------------------ queue

void Player::buildQueue(QueueKind kind, int32_t arg, int32_t startSong) {
    order_.clear();
    if (kind == QueueKind::Folder && arg >= 0 && (size_t)arg < lib_.folderCount()) {
        const Folder& f = lib_.folder(arg);
        for (uint16_t i = 0; i < f.count; ++i) order_.push_back(f.first + i);
    } else {
        kind = QueueKind::AllAZ;
        arg = 0;
        for (size_t k = 0; k < lib_.count(); ++k) order_.push_back(lib_.az(k));
    }
    qkind_ = kind;
    qarg_ = arg;

    size_t start = 0;
    for (size_t i = 0; i < order_.size(); ++i)
        if (order_[i] == startSong) { start = i; break; }

    if (shuffle_ && order_.size() > 1) {
        // Chosen song first, rest Fisher–Yates shuffled: every song once before any repeats.
        std::swap(order_[0], order_[start]);
        for (size_t i = order_.size() - 1; i > 1; --i) {
            size_t j = 1 + esp_random() % i;  // j in [1, i]
            std::swap(order_[i], order_[j]);
        }
        start = 0;
    }
    qpos_ = start;
}

void Player::setShuffle(bool on) {
    if (on == shuffle_) return;
    shuffle_ = on;
    prefs.putBool("shuf", on);
    if (!order_.empty()) buildQueue(qkind_, qarg_, order_[qpos_]);  // keeps the current song playing
    publishQueue();
}

void Player::publishQueue() {
    portENTER_CRITICAL(&mux_);
    st_.shuffle = shuffle_;
    st_.repeat = repeat_;
    st_.qpos = (uint16_t)qpos_;
    st_.qlen = (uint16_t)order_.size();
    portEXIT_CRITICAL(&mux_);
}

// ------------------------------------------------------------------ playback

bool Player::play(int32_t song, uint32_t filePos) {
    if (!lib_.path(song, curPath_, sizeof curPath_)) return false;

    char name[128];
    MusicLibrary::displayName(curPath_, name, sizeof name);
    portENTER_CRITICAL(&mux_);
    st_.track = song;
    st_.posSec = st_.durSec = 0;
    strlcpy(st_.title, name, sizeof st_.title);  // fallback until ID3 arrives
    st_.artist[0] = '\0';
    st_.metaSeq++;
    portEXIT_CRITICAL(&mux_);
    publishQueue();

    Serial.printf("[player] #%ld %s @%lu\n", (long)song, curPath_, (unsigned long)filePos);
    bool ok = audio_.connecttoFS(SD, curPath_, filePos);
    setPaused(false);

    prefs.putString("path", curPath_);
    if (prefs.getULong("fpos", 0) != 0) prefs.putULong("fpos", 0);
    return ok;
}

// dir: +1 next, -1 previous. fromEof: track ended by itself (repeat-one applies).
void Player::step(int dir, bool fromEof) {
    const size_t n = order_.size();
    if (!n) return;
    if (fromEof && repeat_ == Repeat::One) { play(order_[qpos_]); return; }

    for (size_t tries = 0; tries < n; ++tries) {  // skip unplayable files, max one lap
        long p = (long)qpos_ + dir;
        bool wrapped = false;
        if (p >= (long)n) { p = 0; wrapped = true; }
        if (p < 0) {
            if (repeat_ != Repeat::All) { play(order_[qpos_]); return; }  // at start: restart song
            p = (long)n - 1;
        }
        qpos_ = (size_t)p;
        if (play(order_[qpos_])) {
            if (wrapped && repeat_ == Repeat::Off) setPaused(true);  // end of queue: stop on first song
            return;
        }
    }
}

void Player::setPaused(bool p) {
    if (p == paused_) return;
    if (audio_.isRunning() == p) audio_.pauseResume();
    paused_ = p;
    if (cfg::PIN_DAC_XSMT >= 0) digitalWrite(cfg::PIN_DAC_XSMT, p ? LOW : HIGH);
    if (p) prefs.putULong("fpos", audio_.getFilePos());  // resume point survives power-off
    portENTER_CRITICAL(&mux_);
    st_.paused = p;
    portEXIT_CRITICAL(&mux_);
}

void Player::handle(const Msg& m) {
    switch (m.cmd) {
    case PlayerCmd::PlayQueue:
        buildQueue((QueueKind)m.a, m.b, m.c);
        prefs.putUChar("qkind", (uint8_t)qkind_);
        prefs.putInt("qarg", qarg_);
        if (!order_.empty() && !play(order_[qpos_])) step(+1, false);
        break;
    case PlayerCmd::Next: step(+1, false); break;
    case PlayerCmd::Prev:
        // iPod behaviour: restart current song if >3 s in, else go to previous.
        if (audio_.getAudioCurrentTime() > 3 && !order_.empty()) play(order_[qpos_]);
        else step(-1, false);
        break;
    case PlayerCmd::TogglePause: setPaused(!paused_); break;
    case PlayerCmd::Pause:       setPaused(true); break;
    case PlayerCmd::VolumeDelta: {
        int v = constrain((int)volume_ + m.a, 0, (int)cfg::VOLUME_MAX);
        if (v != volume_) {
            volume_ = v;
            audio_.setVolume(volume_);
            volDirtyAt_ = millis() | 1;
            portENTER_CRITICAL(&mux_);
            st_.volume = volume_;
            portEXIT_CRITICAL(&mux_);
        }
        break;
    }
    case PlayerCmd::SetShuffle: setShuffle(m.a != 0); break;
    case PlayerCmd::SetRepeat:
        repeat_ = (Repeat)constrain(m.a, 0, 2);
        prefs.putUChar("rep", (uint8_t)repeat_);
        publishQueue();
        break;
    }
}

void Player::publishTime() {
    uint32_t pos = audio_.getAudioCurrentTime();
    uint32_t dur = audio_.getAudioFileDuration();
    portENTER_CRITICAL(&mux_);
    st_.posSec = pos;
    st_.durSec = dur;
    portEXIT_CRITICAL(&mux_);
}

void Player::run() {
    uint32_t lastPub = 0, lastHwm = 0;
    for (;;) {
        // While paused, block on the queue instead of spinning: CPU idles, saves power.
        Msg m;
        TickType_t wait = paused_ ? pdMS_TO_TICKS(50) : 0;
        while (xQueueReceive(q_, &m, wait) == pdTRUE) { handle(m); wait = 0; }

        audio_.loop();

        if (eofPending_) { eofPending_ = false; step(+1, true); }

        const uint32_t now = millis();
        if (now - lastPub >= 200) {
            lastPub = now;
            publishTime();
            if (volDirtyAt_ && now - volDirtyAt_ > 3000) {  // debounced: knob spins don't hammer flash
                prefs.putUChar("vol", volume_);
                volDirtyAt_ = 0;
            }
        }
        if (now - lastHwm >= 30000) {
            lastHwm = now;
            Serial.printf("[audio] stack free %u B, heap free %u B\n",
                          (unsigned)uxTaskGetStackHighWaterMark(nullptr), (unsigned)ESP.getFreeHeap());
        }
        vTaskDelay(1);
    }
}
