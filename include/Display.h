#ifndef DISPLAY_H
#define DISPLAY_H

#include "TFT_eSPI.h"
#include "SPI.h"
#include "MusicManager.h"


extern TFT_eSPI tft; 

class Display {
    private:
        TFT_eSPI& tft;
        MusicManager& musicManager;
        const int Screen_Width;
        const int Screen_Height;
        unsigned long lastVolumeChange = 0;
        bool volumeBarVisible = false;
        int currentVolume = 0;
        int menuStartIndex = 0;
        int menuSelection = 0;
        const int visibleItems = 8;

    public:
        Display(TFT_eSPI& tftRef, MusicManager& musicRef, int screenWidth, int screenHeight) 
            : tft(tftRef), musicManager(musicRef), Screen_Width(screenWidth), Screen_Height(screenHeight)  {}
        void init();
        void clearScreen();
        void turnOnScreen();
        void turnOffScreen();
        void scanSongs();
        void audioTime();
        void progressBar();
        void showVolumeBar(int volume);
        void updateVolumeBar();
        void menuListSongs();
        void menuSelectSong(int index);
        void audio_info(const char *info);
        void audio_id3data(const char *info);  
        void audio_eof_mp3(const char *info); 
        unsigned long lastUpdateTimeline = 0;
        unsigned long lastInteraction = 0;
        bool screenOn = true;

    
};

#endif