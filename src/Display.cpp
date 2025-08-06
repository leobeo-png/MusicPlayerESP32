#include "Display.h"

void Display::init() {
    tft.init();
    tft.setRotation(3);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);
    tft.loadFont("/fonts/ZenMaruGothic-Regular-24", SD);
}

void Display::clearScreen() {
    tft.fillScreen(TFT_BLACK);
}

void Display::audioTime() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastUpdateTimeline >= 1000) {
        lastUpdateTimeline = currentMillis;

        unsigned long seconds = audio.getAudioCurrentTime();
        unsigned long minutes = seconds / 60;
        seconds = seconds % 60;
        char timeStr[6];
        sprintf(timeStr, "%02lu:%02lu", minutes, seconds);

        tft.fillRect(0, 100, 80, 20, TFT_BLACK);
        tft.setCursor(10, 100);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(2);
        tft.print(timeStr);
    }       
    
}


void Display::audio_info(const char *info){
    Serial.print("info        ");Serial.println(info);
}

void Display::audio_id3data(const char *info){  //id3 metadata
    Serial.print("id3data     ");Serial.println(info);

    if(strstr(info, "Title: ") == info) {
        const char* title = info + 7; // Skip "Title: "
        tft.fillRect(0, 10, Screen_Width, 40, TFT_BLACK);
        
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setCursor(10, 10);
        tft.println(title);
    }

    if(strstr(info, "Artist: ") == info) {
        String Artist = String(info + 8); // Skip "Artist: "
        tft.fillRect(0, 50, Screen_Width, 20, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(10, 50);
        tft.println(Artist);
    }
}