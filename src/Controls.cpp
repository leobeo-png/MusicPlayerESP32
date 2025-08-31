#include "Controls.h"

Controls* Controls::instance = nullptr;

Controls::Controls(Audio& audioRef, MusicManager& musicRef, Display& displayRef) 
    : audio(audioRef), musicManager(musicRef), display(displayRef), currentSongIndex(0) {
    instance = this; // Set static instance for ISR
    encoderPos = 0;
    lastInterruptTime = 0;

}

void Controls::controlPins() {
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
    static bool lastButtonState = HIGH;
    
    if (encoderPos != lastEncoderPos) {
        int diff = encoderPos - lastEncoderPos;
        bool pressed = (digitalRead(Encoder_Button) == LOW);

        if (diff > 0) {
            if(pressed) {
                playNext();
            } else {
                VolumeUp();
            }
            inputDetected = true;
        } else if (diff < 0) {
            if(pressed) {
                playPrev();
            } else {
                VolumeDown();
            }
            inputDetected = true;
        }
        
        lastEncoderPos = encoderPos;
    }

    unsigned long currentMillis = millis();

    if (currentMillis - lastButtonCheck >= buttonCheckInterval) {
        lastButtonCheck = currentMillis;
        bool currentButtonCheck = digitalRead(Encoder_Button);

        if (lastButtonState == HIGH && currentButtonCheck == LOW) {
            audio.pauseResume();
            display.pauseIcon(!audio.isRunning());
            inputDetected = true;
        }
        lastButtonState = currentButtonCheck;
    }
    if (inputDetected) {
        display.lastInteraction = millis(); 
        if (!display.screenOn) {
            display.turnOnScreen();
        }
    }
}