// ESP32 MP3 player
//   core 0: audio task (Player)  — decode + I2S, never blocked by the display
//   core 1: Arduino loop()       — input, UI, battery, power
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <driver/rtc_io.h>
#include "Config.h"
#include "MusicLibrary.h"
#include "Player.h"
#include "Input.h"
#include "Ui.h"
#include "Battery.h"

static TFT_eSPI     tft;
static MusicLibrary library;
static Player       player(library);
static Input        input;
static Ui           ui(tft, library, player);
static Battery      battery;

static uint32_t pausedSince = 0;  // 0 = playing

static bool belowCutoff(uint16_t mv, uint16_t cutoff) {
    return cutoff && mv > 1000 && mv < cutoff;  // >1000: ignore "no divider / USB only" readings
}

[[noreturn]] static void deepSleep(const char* why) {
    Serial.printf("[pwr] deep sleep: %s\n", why);
    player.send(PlayerCmd::Pause);  // pausing saves the resume position to NVS
    delay(150);
    ui.wake();
    ui.message(why, "Press knob to wake");
    delay(1500);
    ui.sleepPanel();
    rtc_gpio_pullup_en((gpio_num_t)cfg::PIN_ENC_SW);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)cfg::PIN_ENC_SW, 0);
    Serial.flush();
    esp_deep_sleep_start();
}

static void mountSd() {
    while (!SD.begin(cfg::PIN_SD_CS, SPI, cfg::SD_SPI_HZ)) {
        Serial.println("[sd] mount failed");
        ui.message("No SD card", "Insert card, retrying...");
        delay(2000);
    }
}

void setup() {
    Serial.begin(115200);
    setCpuFrequencyMhz(cfg::CPU_MHZ);

    pinMode(cfg::PIN_SD_CS, OUTPUT);
    digitalWrite(cfg::PIN_SD_CS, HIGH);  // keep SD off the bus while the panel initialises
    SPI.begin(cfg::PIN_SPI_SCK, cfg::PIN_SPI_MISO, cfg::PIN_SPI_MOSI);

    ui.begin();
    input.begin();
    mountSd();
    ui.loadFont();

    const bool rescan = cfg::RESCAN_ON_BOOT || input.buttonHeld() || Ui::takeRescanFlag();
    ui.message(rescan ? "Scanning library..." : "Loading library...");
    library.begin(rescan);
    while (library.count() == 0) {
        ui.message("No songs found", "Add files to /music, click to rescan");
        while (input.poll().ev != InputEvent::Click) delay(10);
        ui.message("Scanning library...");
        library.begin(true);
    }

    battery.begin();
    player.begin();
    ui.showHome();
}

void loop() {
    for (InputMsg m; (m = input.poll()).ev != InputEvent::None;) ui.handle(m);

    const PlayerStatus s = player.status();
    battery.update();
    ui.updatePower();
    ui.render(s, battery.percent(), battery.millivolts());

    // ---- power policy ----
    const uint32_t now = millis();
    if (s.paused) { if (!pausedSince) pausedSince = now | 1; }
    else pausedSince = 0;
    if (cfg::SLEEP_AFTER_PAUSE_MS && pausedSince && !ui.awake() &&
        now - pausedSince > cfg::SLEEP_AFTER_PAUSE_MS)
        deepSleep("Sleeping");
    if (ui.sleepTimerDue()) deepSleep("Sleep timer");
    if (belowCutoff(battery.millivolts(), cfg::VBAT_CUTOFF_MV)) deepSleep("Battery low");

    vTaskDelay(pdMS_TO_TICKS(5));  // ~200 Hz UI loop; drawing only happens on change
}
