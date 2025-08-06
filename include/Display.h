#ifndef DISPLAY_H
#define DISPLAY_H

#include "TFT_eSPI.h"
#include "SPI.h"
#include "MusicManager.h"


extern TFT_eSPI tft; 

class Display {
    private:
        TFT_eSPI& tft;
        const int Screen_Width;
        const int Screen_Height;

    public:
        Display(TFT_eSPI& tftRef, int screenWidth, int screenHeight = 240) 
            : tft(tftRef), Screen_Width(screenWidth), Screen_Height(screenHeight) {}
        void init();
        void clearScreen();
        void audioTime();
        void audio_info(const char *info);
        void audio_id3data(const char *info);  
        unsigned long lastUpdateTimeline = 0;

    
};

#endif