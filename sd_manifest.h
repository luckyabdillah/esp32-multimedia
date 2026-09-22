#pragma once

#include <Arduino.h>

// Initialize SPI bus for SD and call SD.begin() with several attempts.
// Return false if SD still fails after all attempts.
bool sdInit();

// Scan /gif and /audio, match basenames that have a gif+wav PAIR with
// the same name, and store the result in manifestItems[].
void buildManifest();

// Write current manifestItems[] to manifest.json in SD card.
bool writeManifestJSON();

// Re-read manifest.json from SD into manifestItems[] (used as the
// source for randomize, not to re-scan the directories).
bool loadManifestJSON();

// Choose a random basename from manifestItems[], avoiding `avoid` if
// possible. Return empty string if manifest is empty.
String pickRandomBasename(const String &avoid);
