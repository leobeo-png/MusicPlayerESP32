#pragma once
// Owns the Audio object and runs it in a dedicated FreeRTOS task.
// Other tasks never touch Audio directly: they send commands and read a status snapshot.
#include <Arduino.h>
#include <Audio.h>
#include <vector>
#include "MusicLibrary.h"

enum class PlayerCmd : uint8_t {
    PlayQueue,    // a = QueueKind, b = folder index (Folder queue), c = song to start with
    Next, Prev, TogglePause, Pause,
    VolumeDelta,  // a = delta
    SetShuffle,   // a = 0/1
    SetRepeat,    // a = Repeat
};
enum class QueueKind : uint8_t { AllAZ = 0, Folder = 1 };
enum class Repeat : uint8_t { Off = 0, All = 1, One = 2 };

struct PlayerStatus {
    int32_t  track    = -1;
    uint32_t posSec   = 0;
    uint32_t durSec   = 0;
    uint8_t  volume   = 0;
    bool     paused   = false;
    bool     shuffle  = false;
    Repeat   repeat   = Repeat::Off;
    uint16_t qpos     = 0;   // position in current queue
    uint16_t qlen     = 0;
    uint32_t metaSeq  = 0;   // bumps when track/title/artist change -> UI redraws text
    char     title[128] = "";
    char     artist[64] = "";
};

class Player {
public:
    explicit Player(MusicLibrary& lib) : lib_(lib) {}

    void begin();  // restores volume/shuffle/repeat/queue/position from NVS, starts the task

    void send(PlayerCmd cmd, int32_t a = 0, int32_t b = 0, int32_t c = 0);
    PlayerStatus status();

    // Called from the global audio_* callbacks (inside audio.loop(), i.e. the audio task).
    void onId3(const char* info);
    void onEof() { eofPending_ = true; }

private:
    struct Msg { PlayerCmd cmd; int32_t a, b, c; };

    static void taskEntry(void* self) { static_cast<Player*>(self)->run(); }
    void run();
    void handle(const Msg& m);
    void buildQueue(QueueKind kind, int32_t arg, int32_t startSong);
    bool play(int32_t song, uint32_t filePos = 0);
    void step(int dir, bool fromEof);
    void setPaused(bool p);
    void setShuffle(bool on);
    void publishQueue();
    void publishTime();

    MusicLibrary& lib_;
    Audio audio_;
    QueueHandle_t q_ = nullptr;
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    PlayerStatus st_;            // guarded by mux_

    // audio-task-only state
    std::vector<uint16_t> order_;  // song indices in play order
    size_t qpos_ = 0;
    QueueKind qkind_ = QueueKind::AllAZ;
    int32_t qarg_ = 0;
    bool shuffle_ = false;
    Repeat repeat_ = Repeat::Off;
    char curPath_[256] = "";
    volatile bool eofPending_ = false;
    bool paused_ = false;
    uint8_t volume_ = 0;
    uint32_t volDirtyAt_ = 0;
};
