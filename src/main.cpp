#include "MusicManager.h"
#include "Controls.h"
#include "Display.h"
#include "Pinout.h"

#define SD_CS          5
#define SPI_MOSI      23
#define SPI_MISO      19
#define SPI_SCK       18
#define I2S_DOUT      21
#define I2S_BCLK      16
#define I2S_LRC       17
#define BatteryVoltage 34

unsigned long lastButtonCheck = 0;
const unsigned long buttonCheckInterval = 100; // Check every 200 ms

TFT_eSPI tft = TFT_eSPI(); 
Audio audio;
MusicManager musicManager;
Display display(tft, musicManager, 240, 320);
Controls controls(audio, musicManager, display);



Display* g_display = nullptr;

void setup() {

    pinMode(SD_CS, OUTPUT);  
    pinMode(TFT_CS, OUTPUT);
    controls.controlPins();
    

    digitalWrite(SD_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);
    
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    Serial.begin(115200);
    
    if (!SD.begin(SD_CS)) {
        Serial.println("SD Card initialization failed!");
        return;
    }

    display.init();
    g_display = &display;

    musicManager.buildSongIndex("/music", "/song_index.txt");
    display.scanSongs();

    audio.setVolume(14);
    controls.playSong(0);

    display.clearScreen();
    display.menuListSongs();
}


void loop(){
    audio.loop();
    controls.checkEncoderChange();
    display.audioTime(); 
    display.progressBar();
    display.updateVolumeBar();
    display.batteryLevel(BatteryVoltage);
    if(display.screenOn && millis() - display.lastInteraction > 30000) {
        display.turnOffScreen(); 
    }
    vTaskDelay(1);
}

void audio_info(const char *info) {
    if(g_display) g_display->audio_info(info);
}

void audio_id3data(const char *info) {
    if(g_display) g_display->audio_id3data(info);
}

void audio_eof_mp3(const char *info){  //end of file
    if(g_display) g_display->audio_eof_mp3(info);
    controls.playNext(); 
}

