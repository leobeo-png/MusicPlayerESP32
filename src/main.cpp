#include "MusicManager.h"
#include "Controls.h"
#include "Display.h"
#include "Pinout.h"

#define SD_CS          5
#define SPI_MOSI      23
#define SPI_MISO      19
#define SPI_SCK       18
#define I2S_DOUT      16
#define I2S_BCLK      21
#define I2S_LRC       17

unsigned long lastButtonCheck = 0;
const unsigned long buttonCheckInterval = 100; // Check every 200 ms

TFT_eSPI tft = TFT_eSPI(); 
Audio audio;
MusicManager musicManager;
Display display(tft, 320, 240);
Controls controls(audio, musicManager, display);


// global instance pointer for callbacks
Display* g_display = nullptr;

void setup() {
    // Pin setup
    pinMode(SD_CS, OUTPUT);  
    pinMode(TFT_CS, OUTPUT);
    controls.controlPins();
    
    // Initialize with both CS pins HIGH (deselected)
    digitalWrite(SD_CS, HIGH);
    digitalWrite(TFT_CS, HIGH);
    
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    Serial.begin(115200);
    
    if (!SD.begin(SD_CS)) {
        Serial.println("SD Card initialization failed!");
        return;
    }
    
    // Read SD contents while SD is initialized
    musicManager.readSD();
    
    // Initialize TFT
    display.init();
    g_display = &display; // Set global display pointer
    
    // Audio settings
    audio.setVolume(14);
    controls.playSong(0);
}


void loop(){
    audio.loop();
    display.audioTime(); 
    display.progressBar();
    controls.debounceButton(); 
    controls.checkEncoderChange();
    display.updateVolumeBar();
    vTaskDelay(2);
}

void audio_info(const char *info) {
    if(g_display) g_display->audio_info(info);
}

void audio_id3data(const char *info) {
    if(g_display) g_display->audio_id3data(info);
}

void audio_eof_mp3(const char *info){  //end of file
    if(g_display) g_display->audio_info(info);
    controls.playNext(); 
}