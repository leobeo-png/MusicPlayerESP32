#pragma once
// Owns the Audio object and runs it in a dedicated FreeRTOS task.
// Other tasks never touch Audio directly: they send commands and read a status snapshot.
#include <Arduino.h>
#include <Audio.h>
#include "MusicLibrary.h"

enum class PlayerCmd : uint8_t { PlayIndex, Next, Prev, TogglePause, VolumeDelta };

struct PlayerStatus {
    int32_t  track    = -1;
    uint32_t posSec   = 0;
    uint32_t durSec   = 0;
    uint8_t  volume   = 0;
    bool     paused   = false;
    uint32_t metaSeq  = 0;   // bumps when track/title/artist change -> UI redraws text
    char     title[128] = "";
    char     artist[64] = "";
};

class Player {
public:
    explicit Player(MusicLibrary& lib) : lib_(lib) {}

    // Starts the audio task. Resumes the last track/position saved in NVS.
    void begin();

    void send(PlayerCmd cmd, int32_t arg = 0);
    PlayerStatus status();   // copy, safe from any task

    // Called from the global audio_* callbacks (run inside audio.loop(), i.e. the audio task).
    void onId3(const char* info);
    void onEof() { eofPending_ = true; }

private:
    struct Msg { PlayerCmd cmd; int32_t arg; };

    static void taskEntry(void* self) { static_cast<Player*>(self)->run(); }
    void run();
    void handle(const Msg& m);
    bool play(int32_t index, uint32_t filePos = 0);
    void step(int dir);
    void setPaused(bool p);
    void publishTime();
    void persist(uint32_t now);

    MusicLibrary& lib_;
    Audio audio_;
    QueueHandle_t q_ = nullptr;
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    PlayerStatus st_;            // guarded by mux_
    char curPath_[256] = "";
    volatile bool eofPending_ = false;
    bool paused_ = false;
    uint8_t volume_ = 0;
    uint32_t volDirtyAt_ = 0;    // 0 = clean
};
