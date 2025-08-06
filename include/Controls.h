#ifndef CONTROLS_H
#define CONTROLS_H

#include "Arduino.h"
#include "Audio.h"
#include "MusicManager.h"

class Controls {
    public:
        Controls(Audio& audioRef, MusicManager& musicRef);
        void playSong(int index);
        void playNext();
        void playPrev();
        void VolumeUp();
        void VolumeDown();
        void audioTime();

    private:
        Audio& audio;
        MusicManager& musicManager;
        int currentSongIndex = 0;
        
};

#endif