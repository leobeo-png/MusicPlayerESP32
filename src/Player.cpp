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
    volume_ = min<uint8_t>(prefs.getUChar("vol", cfg::VOLUME_DEFAULT), cfg::VOLUME_MAX);
    audio_.setVolume(volume_);
    st_.volume = volume_;

    // Resume last track (by path, so a rescan that reorders files still finds it).
    String last = prefs.getString("path", "");
    uint32_t fpos = prefs.getULong("fpos", 0);
    int idx = lib_.find(last.c_str());
    if (idx < 0) { idx = 0; fpos = 0; }
    if (lib_.count() && !play(idx, fpos)) step(+1);

    xTaskCreatePinnedToCore(taskEntry, "audio", cfg::AUDIO_STACK, this, cfg::AUDIO_PRIO, nullptr, cfg::AUDIO_CORE);
}

void Player::send(PlayerCmd cmd, int32_t arg) {
    Msg m{cmd, arg};
    xQueueSend(q_, &m, 0);  // drop if full: input is human-rate, never happens in practice
}

PlayerStatus Player::status() {
    PlayerStatus s;
    portENTER_CRITICAL(&mux_);
    s = st_;
    portEXIT_CRITICAL(&mux_);
    return s;
}

void Player::onId3(const char* info) {
    const char* v;
    portENTER_CRITICAL(&mux_);
    if ((v = strstr(info, "Title: ")) == info) {
        strlcpy(st_.title, info + 7, sizeof st_.title);
        st_.metaSeq++;
    } else if ((v = strstr(info, "Artist: ")) == info) {
        strlcpy(st_.artist, info + 8, sizeof st_.artist);
        st_.metaSeq++;
    }
    portEXIT_CRITICAL(&mux_);
}

bool Player::play(int32_t index, uint32_t filePos) {
    if (!lib_.path(index, curPath_, sizeof curPath_)) return false;

    char name[128];
    MusicLibrary::displayName(curPath_, name, sizeof name);
    portENTER_CRITICAL(&mux_);
    st_.track = index;
    st_.posSec = st_.durSec = 0;
    strlcpy(st_.title, name, sizeof st_.title);  // fallback until ID3 arrives
    st_.artist[0] = '\0';
    st_.metaSeq++;
    portEXIT_CRITICAL(&mux_);

    Serial.printf("[player] #%ld %s @%lu\n", (long)index, curPath_, (unsigned long)filePos);
    bool ok = audio_.connecttoFS(SD, curPath_, filePos);
    setPaused(false);

    prefs.putString("path", curPath_);
    if (prefs.getULong("fpos", 0) != 0) prefs.putULong("fpos", 0);
    return ok;
}

void Player::step(int dir) {
    const int32_t n = (int32_t)lib_.count();
    if (n == 0) return;
    int32_t i = status().track;
    // Skip unplayable files, but give up after one full lap.
    for (int32_t tries = 0; tries < n; ++tries) {
        i = ((i + dir) % n + n) % n;
        if (play(i)) return;
    }
}

void Player::setPaused(bool p) {
    if (p == paused_) return;
    if (audio_.isRunning() == p) audio_.pauseResume();  // running && want pause, or stopped && want resume
    paused_ = p;
    if (cfg::PIN_DAC_XSMT >= 0) digitalWrite(cfg::PIN_DAC_XSMT, p ? LOW : HIGH);
    if (p) prefs.putULong("fpos", audio_.getFilePos());  // resume point survives power-off
    portENTER_CRITICAL(&mux_);
    st_.paused = p;
    portEXIT_CRITICAL(&mux_);
}

void Player::handle(const Msg& m) {
    switch (m.cmd) {
    case PlayerCmd::PlayIndex:   play(m.arg); break;
    case PlayerCmd::Next:        step(+1); break;
    case PlayerCmd::Prev:
        // iPod behaviour: restart current song if >3 s in, else go to previous.
        if (audio_.getAudioCurrentTime() > 3) play(status().track);
        else step(-1);
        break;
    case PlayerCmd::TogglePause: setPaused(!paused_); break;
    case PlayerCmd::VolumeDelta: {
        int v = constrain((int)volume_ + m.arg, 0, (int)cfg::VOLUME_MAX);
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

void Player::persist(uint32_t now) {
    // Debounced so spinning the knob doesn't hammer flash.
    if (volDirtyAt_ && now - volDirtyAt_ > 3000) {
        prefs.putUChar("vol", volume_);
        volDirtyAt_ = 0;
    }
}

void Player::run() {
    uint32_t lastPub = 0;
    uint32_t lastHwm = 0;
    for (;;) {
        // While paused, block on the queue instead of spinning: CPU idles, saves power.
        Msg m;
        TickType_t wait = paused_ ? pdMS_TO_TICKS(50) : 0;
        while (xQueueReceive(q_, &m, wait) == pdTRUE) { handle(m); wait = 0; }

        audio_.loop();

        if (eofPending_) { eofPending_ = false; step(+1); }

        uint32_t now = millis();
        if (now - lastPub >= 200) { lastPub = now; publishTime(); persist(now); }
        if (now - lastHwm >= 30000) {
            lastHwm = now;
            Serial.printf("[audio] stack free %u B, heap free %u B\n",
                          (unsigned)uxTaskGetStackHighWaterMark(nullptr), (unsigned)ESP.getFreeHeap());
        }
        vTaskDelay(1);
    }
}
