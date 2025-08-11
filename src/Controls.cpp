#include "Controls.h"

Controls* Controls::instance = nullptr;

Controls::Controls(Audio& audioRef, MusicManager& musicRef, Display& displayRef) 
    : audio(audioRef), musicManager(musicRef), display(displayRef), currentSongIndex(0) {
    instance = this; // Set static instance for ISR
    encoderPos = 0;
    lastInterruptTime = 0;

}

void Controls::controlPins() {
    pinMode(Next_Song_Button, INPUT_PULLUP);
    pinMode(Previous_Song_Button, INPUT_PULLUP);
    pinMode(Play_Pause_Button, INPUT_PULLUP);
    pinMode(Encoder_Volume_Up, INPUT_PULLUP);
    pinMode(Encoder_Volume_Down, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(Encoder_Volume_Up), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(Encoder_Volume_Down), encoderISR, CHANGE);
}

void Controls::playSong(int index) {
    if (index < 0 || index >= musicManager.getSongCount("/song_index.txt")) {
        return;
    }
    String songPath = musicManager.getSongByIndex("/song_index.txt", index);
    Serial.println("Playing: " + songPath);
    audio.connecttoFS(SD, songPath.c_str());
}

void Controls::playNext() {
    currentSongIndex = (currentSongIndex + 1) % musicManager.getSongCount("/song_index.txt");
    playSong(currentSongIndex);
    display.menuSelectSong(currentSongIndex); 
}

void Controls::playPrev() {
    currentSongIndex = (currentSongIndex - 1 + musicManager.getSongCount("/song_index.txt")) % musicManager.getSongCount("/song_index.txt");
    playSong(currentSongIndex);
    display.menuSelectSong(currentSongIndex); 
}

void Controls::VolumeUp() {
    int vol = audio.getVolume() + 1;
    audio.setVolume(vol > 21 ? 21 : vol); 
    Serial.println(audio.getVolume());
    display.showVolumeBar(audio.getVolume()); 
}

void Controls::VolumeDown() {
    int vol = audio.getVolume() - 1;
    audio.setVolume(vol < 0 ? 0 : vol);
    Serial.println(audio.getVolume());
    display.showVolumeBar(audio.getVolume()); 
}


void Controls::debounceButton() {
    bool inputDetected = false;
    unsigned long currentMillis = millis();

    if (currentMillis - lastButtonCheck >= buttonCheckInterval) {
        lastButtonCheck = currentMillis;
        if (digitalRead(Next_Song_Button) == LOW) {
            playNext();
            inputDetected = true;
        }
        if (digitalRead(Previous_Song_Button) == LOW) {
            playPrev();
            inputDetected = true;
        }
        if (digitalRead(Play_Pause_Button) == LOW) {
            audio.pauseResume();
            inputDetected = true;
        }
    }
    if (inputDetected) {
        display.lastInteraction = millis(); 
        if (!display.screenOn) {
            display.turnOnScreen();
        }
    }
}

void IRAM_ATTR Controls::encoderISR() {
    if (instance) {
        unsigned long currentTime = micros();
        if (currentTime - instance->lastInterruptTime > 2000) { 
            int clkState = digitalRead(Encoder_Volume_Up);
            int dtState = digitalRead(Encoder_Volume_Down);
            
            if (clkState != instance->lastClkState) {
                if (dtState != clkState) {
                    instance->encoderPos++;  // Clockwise
                } else {
                    instance->encoderPos--;  // Counter-clockwise
                }
            }
            instance->lastClkState = clkState;
            instance->lastInterruptTime = currentTime;
        }
    }
}

void Controls::checkEncoderChange() {
    bool inputDetected = false;
    static int lastEncoderPos = 0;
    
    if (encoderPos != lastEncoderPos) {
        int diff = encoderPos - lastEncoderPos;
        
        if (diff > 0) {
            VolumeUp();
            inputDetected = true;
        } else if (diff < 0) {
            VolumeDown();
            inputDetected = true;
        }
        
        lastEncoderPos = encoderPos;
    }

    if (inputDetected) {
        display.lastInteraction = millis(); 
        if (!display.screenOn) {
            display.turnOnScreen();
        }
    }
}