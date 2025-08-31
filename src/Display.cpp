#include "Display.h"



void Display::init() {
    turnOnScreen();
    tft.init();
    tft.setRotation(2);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);
    tft.loadFont("/fonts/ZenMaruGothic-Regular-16-minJap-noSmooth", SD);
}

void Display::clearScreen() {
    tft.fillScreen(TFT_BLACK);
}

void Display::turnOnScreen() {
    digitalWrite(TFT_BL, HIGH); 
    screenOn = true;
}

void Display::turnOffScreen() {
    digitalWrite(TFT_BL, LOW);
    screenOn = false; 
}


void Display::scanSongs() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(50, 100);
    tft.println("Scanning for songs...");
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
    unsigned long currentMillis = millis(); // updates each second
    
    unsigned long totalTime = audio.getAudioFileDuration();
    unsigned long currentTime = audio.getAudioCurrentTime();

    if (totalTime == 0) {
        return; // avoid division by zero
    }

    int progress = (int((uint64_t)(Screen_Width - 20) * currentTime / totalTime));

    if(progress != lastBarWidth) {
        //clear only previous part if shrinking
        if (progress < lastBarWidth) {
            tft.fillRect(10 + progress, 130, lastBarWidth - progress, 10, TFT_BLACK);
        } else {
            tft.fillRect(10, 130, progress, 10, TFT_WHITE);
           
        }      

        tft.drawRect(10, 130, Screen_Width - 20, 10, TFT_WHITE);
        
        lastBarWidth = progress;
    }

}

void Display::showVolumeBar(int volume) {
    currentVolume = volume;
    lastVolumeChange = millis();
    volumeBarVisible = true;

    int barWidth = map(volume, 0, 21, 0, 40);
    tft.fillRect(Screen_Width - 50, 150, 60, 10, TFT_BLACK);
    tft.fillRect(Screen_Width - 50, 150, barWidth, 10, TFT_CYAN);
    tft.drawRect(Screen_Width - 50, 150, 60 - 20, 10, TFT_WHITE);    
}

void Display::updateVolumeBar() {
    if (volumeBarVisible && (millis() - lastVolumeChange > 2000)) {
        tft.fillRect(Screen_Width - 52, 148, 62, 12, TFT_BLACK);
        volumeBarVisible = false;
    }
}

void Display::menuListSongs() {
    int yPos = 240;
    tft.fillRect(0, yPos - 2, Screen_Width, Screen_Height - (yPos - 2), TFT_BLACK);

    for (int i = 0; i < visibleItems; i++) {
        int songIndex = menuStartIndex + i;
        if (songIndex >= musicManager.getSongCount("/song_index.txt")) {
            break; // No more songs to display
        }
        String songName = musicManager.getSongByIndex("/song_index.txt", songIndex);

        int lastSlash = songName.lastIndexOf('/');
        if (lastSlash >= 0) {
            songName = songName.substring(lastSlash + 1 );
        }

        if (songIndex == menuSelection) {
            tft.setTextColor(TFT_BLACK, TFT_CYAN, true);
        } else {
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
        }

        tft.setCursor(10, yPos + i * 20);
        tft.setTextWrap(false);
        tft.print(songName);
    }

}


void Display::menuSelectSong(int index) {
    int songCount = musicManager.getSongCount("/song_index.txt");
    if (index < 0) index = 0;
    if (index >= songCount) {
        index = songCount - 1;
    }
    menuSelection = index;

    

    if (menuSelection < menuStartIndex) {
        menuStartIndex = menuSelection; // Adjust start index if selection is above current start
    } else if (menuSelection >= menuStartIndex + visibleItems) {
        menuStartIndex = menuSelection - (visibleItems - 3); // Adjust start index if selection is below current start
    }

    if (menuStartIndex > songCount - visibleItems) {
        menuStartIndex = max(0, songCount - visibleItems);
    }
    menuListSongs();
}


void Display::batteryLevel(int pin) {
    int raw = analogRead(pin);
    float adcVoltage = ((float)raw * Vref) / ADC_MAX; 
    float batteryVoltage = adcVoltage / K;

    int batteryPercent = (int)(((batteryVoltage - VBatteryMin) / (VBatteryMax - VBatteryMin)) * 100);
    batteryPercent = constrain(batteryPercent, 0, 100);

    unsigned long currentMillis = millis();
    if (currentMillis - lastProgressUpdate >= 5000) { // Update every minute
        lastProgressUpdate = currentMillis;
        tft.fillRect(Screen_Width - 30, 8, 26, 16, TFT_BLACK);
        tft.drawRect(Screen_Width - 28, 10, 20, 12, TFT_WHITE); 
        tft.fillRect(Screen_Width - 8, 14, 4, 4, TFT_WHITE);
        
        int fillWidth = map(batteryPercent, 0, 100, 0, 18);
        if (fillWidth > 0) {
            tft.fillRect(Screen_Width - 27, 11, fillWidth, 10, TFT_GREEN);
        }
    }
}

void Display::pauseIcon(bool paused) {
    if (paused) {
        tft.fillRect(Screen_Width - 30, 90, 25, 30, TFT_BLACK);
        tft.fillRect(Screen_Width - 25, 95, 5, 20, TFT_WHITE);
        tft.fillRect(Screen_Width - 15, 95, 5, 20, TFT_WHITE);
    } else {
        tft.fillRect(Screen_Width - 30, 90, 20, 30, TFT_BLACK);
        tft.fillTriangle(Screen_Width - 25, 95, Screen_Width - 25, 115, Screen_Width - 10, 105, TFT_WHITE);
    }
}



void Display::audio_info(const char *info){
    
}

void Display::audio_id3data(const char *info){  //id3 metadata
    tft.setTextWrap(true);
    if(strstr(info, "Title: ") == info) {
        String Title = String(info + 7); // Skip "Artist: "
        tft.fillRect(0, 28, Screen_Width, 43, TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setCursor(10, 30);
        tft.println(Title);
    }

    if(strstr(info, "Artist: ") == info) {
        String Artist = String(info + 8); 
        tft.fillRect(0, 73, Screen_Width, 22, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(10, 75);
        tft.println(Artist);
    }

    Serial.print("id3 ");Serial.println(info);
}

void Display::audio_eof_mp3(const char *info){  //end of file
    // flag for repeat
}


