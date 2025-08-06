#ifndef CONTROLS_H
#define CONTROLS_H

#include "Arduino.h"
#include "Audio.h"
#include "MusicManager.h"
#include "Display.h"

#define Encoder_Volume_Up  26
#define Encoder_Volume_Down  27
#define Next_Song_Button  25
#define Previous_Song_Button  33
#define Play_Pause_Button 14

class Controls {
    public:
        Controls(Audio& audioRef, MusicManager& musicRef, Display& displayRef);
        void controlPins();
        void playSong(int index);
        void playNext();
        void playPrev();
        void VolumeUp();
        void VolumeDown();
        void audioTime();
        void debounceButton();
        static void IRAM_ATTR encoderISR();
        void checkEncoderChange(); 

    private:
        Audio& audio;
        MusicManager& musicManager;
        Display& display;
        int currentSongIndex = 0;
        int lastButtonCheck = 0;
        const int buttonCheckInterval = 200;
        static Controls* instance;
        volatile int encoderPos;
        volatile unsigned long lastInterruptTime;
        int lastClkState = 0; 
};

#endif