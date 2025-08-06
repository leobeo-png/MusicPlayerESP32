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
    if (index < 0 || index >= musicManager.getSongCount()) {
        return;
    }
    String songPath = musicManager.getSongByIndex(index);
    Serial.println("Playing: " + songPath);
    audio.connecttoFS(SD, songPath.c_str());
}

void Controls::playNext() {
    currentSongIndex = (currentSongIndex + 1) % musicManager.getSongCount();
    playSong(currentSongIndex);
}

void Controls::playPrev() {
    currentSongIndex = (currentSongIndex - 1 + musicManager.getSongCount()) % musicManager.getSongCount();
    playSong(currentSongIndex);
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
    unsigned long currentMillis = millis();

    if (currentMillis - lastButtonCheck >= buttonCheckInterval) {
        lastButtonCheck = currentMillis;
        if (digitalRead(Next_Song_Button) == LOW) {
            playNext();
        }
        if (digitalRead(Previous_Song_Button) == LOW) {
            playPrev();
        }
        if (digitalRead(Play_Pause_Button) == LOW) {
            audio.pauseResume();
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
    static int lastEncoderPos = 0;
    
    if (encoderPos != lastEncoderPos) {
        int diff = encoderPos - lastEncoderPos;
        
        if (diff > 0) {
            VolumeUp();
        } else if (diff < 0) {
            VolumeDown();
        }
        
        lastEncoderPos = encoderPos;
    }
}