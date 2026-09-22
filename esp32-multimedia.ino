/*
====================================================================
 ESP32 MULTIMEDIA PLAYER (GIF + WAV)
 GIF & audio ARE PAIRED based on the same filename
====================================================================

See README.md for the SD card folder structure, wiring, and required
libraries. This file is intentionally kept thin (setup/loop only) -
the actual logic is implemented in the following modules:

config.h              all pins & constants
globals.h/.cpp        shared state & objects between modules
display.h/.cpp        TFT ST7789 driver (low-level)
gif_player.h/.cpp     AnimatedGIF callback, opens/draws GIFs from SD
sd_manifest.h/.cpp    SD scan, pairs gif+wav, writes/reads manifest.json
audio_player.h/.cpp   WAV parsing, I2S, audio task (core 0)
player_control.h/.cpp glue: selects & plays one gif+wav pair
button.h/.cpp         button detection (short press = randomize)

BEHAVIOR
--------
- Boot: select 1 random pair (e.g. 007.gif + 007.wav) and play it.
- GIF loops continuously without stopping.
- Audio plays ONCE and then stops (silence); the GIF keeps running.
- Button press -> randomize to another pair; currently playing audio
  is immediately stopped.
====================================================================
*/


#include "esp_system.h"

#include "config.h"
#include "globals.h"
#include "display.h"
#include "gif_player.h"
#include "sd_manifest.h"
#include "audio_player.h"
#include "player_control.h"
#include "button.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== ESP32 MULTIMEDIA PLAYER (GIF + WAV) ===");

  sdMutex = xSemaphoreCreateMutex();
  if (sdMutex == NULL) {
    Serial.println("Failed to create SD mutex!");
    while (true) delay(1000);
  }

  randomSeed((unsigned long)esp_random());

  buttonInit();
  tftInit();

  if (!sdInit()) {
    fillScreen(0xF800);
    while (true) delay(1000);
  }

  // Scan -> pair gif+wav -> write manifest -> re-read manifest
  buildManifest();
  writeManifestJSON();
  loadManifestJSON();

  audioInit();

  Serial.println("SD mutex active: access to GIF/audio is serialized per SD operation.");

  playRandomPair();

  Serial.println("Setup complete.");
}

void loop() {
  handleButton();
  reopenGifIfNeeded();

  if (!gifReady) {
    delay(200);
    return;
  }

  int delayMs = 0;
  int result = gif.playFrame(true, &delayMs);

  if (result < 0) {
    Serial.printf("GIF error %d, reset...\n", gif.getLastError());
    gif.reset();
    delay(50);
    return;
  }

  // Always loop the GIF, even if the audio has finished playing. The GIF will
  // continue to loop until the user presses the button to randomize to a new pair.
  if (result == 0) {
    gif.reset();
  }

  yield();
}
