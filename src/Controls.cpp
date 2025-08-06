#include "Controls.h"


Controls::Controls(Audio& audioRef, MusicManager& musicRef) 
    : audio(audioRef), musicManager(musicRef), currentSongIndex(0) {
    // Constructor
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
    Serial.print("Volume Up: ");
    Serial.println(audio.getVolume());
}

void Controls::VolumeDown() {
    int vol = audio.getVolume() - 1;
    audio.setVolume(vol < 0 ? 0 : vol);
    Serial.print("Volume Down: ");
    Serial.println(audio.getVolume());
}