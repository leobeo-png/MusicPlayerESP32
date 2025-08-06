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

void Display::progressBar() {
    static int lastBarWidth = -1;
    
    unsigned long currentMillis = millis(); // updates each second
    if (currentMillis - lastUpdateTimeline >= 1000) {
        return;  
    }
    unsigned long totalTime = audio.getAudioFileDuration();
    unsigned long currentTime = audio.getAudioCurrentTime();

    if (totalTime == 0) {
        return; // Avoid division by zero
    }

    int progress = (int((uint64_t)(Screen_Width - 20) * currentTime / totalTime));

    if(progress != lastBarWidth) {
        //clear only previous part if shrinking
        if (progress < lastBarWidth) {
            tft.fillRect(10 + progress, 130, lastBarWidth - progress, 10, TFT_BLACK);
        }
        
        //draw new part
        tft.fillRect(10, 130, progress, 10, TFT_WHITE);

        tft.drawRect(10, 130, Screen_Width - 20, 10, TFT_WHITE);
        lastBarWidth = progress;
    }

}

void Display::showVolumeBar(int volume) {
    currentVolume = volume;
    lastVolumeChange = millis();
    volumeBarVisible = true;

    int barWidth = map(volume, 0, 21, 0, 40);
    tft.fillRect(240, 110, 60, 10, TFT_BLACK); // Clear previous
    tft.fillRect(240, 110, barWidth, 10, TFT_CYAN);
    tft.drawRect(240, 110, 60 - 20, 10, TFT_WHITE);
}

void Display::updateVolumeBar() {
    if (volumeBarVisible && (millis() - lastVolumeChange > 2000)) {
        // Hide the bar after 2 seconds
        tft.fillRect(239, 108, 62, 12, TFT_BLACK);
        volumeBarVisible = false;
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

void Display::audio_eof_mp3(const char *info){  //end of file
    // flag for repeat
}