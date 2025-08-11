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
    tft.fillRect(Screen_Width - 40, 110, 60, 10, TFT_BLACK); // Clear previous
    tft.fillRect(Screen_Width - 40, 110, barWidth, 10, TFT_CYAN);
    tft.drawRect(Screen_Width - 40, 110, 60 - 20, 10, TFT_WHITE);
}

void Display::updateVolumeBar() {
    if (volumeBarVisible && (millis() - lastVolumeChange > 2000)) {
        // Hide the bar after 2 seconds
        tft.fillRect(239, 108, 62, 12, TFT_BLACK);
        volumeBarVisible = false;
    }
}

void Display::menuListSongs() {
    
    tft.fillRect(0, 153, Screen_Width, 90, TFT_BLACK);

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

        tft.setCursor(10, 155 + i * 20);
        tft.setTextWrap(false);
        tft.print(songName);
        Serial.println("menu    " + songName);
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


void Display::audio_info(const char *info){
}

void Display::audio_id3data(const char *info){  //id3 metadata
    tft.setTextWrap(true);
    if(strstr(info, "Title: ") == info) {
        String Title = String(info + 7); // Skip "Artist: "
        tft.fillRect(0, 18, Screen_Width, 45, TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setCursor(10, 20);
        tft.println(Title);
    }

    if(strstr(info, "Artist: ") == info) {
        String Artist = String(info + 8); // Skip "Artist: "
        tft.fillRect(0, 73, Screen_Width, 22, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(10, 75);
        tft.println(Artist);
    }
}

void Display::audio_eof_mp3(const char *info){  //end of file
    // flag for repeat
}