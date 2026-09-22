#pragma once

// ====================================================================
// BEHAVIOUR OPTIONS
// ====================================================================

// false = play audio once, then remain silent (default)
// true  = automatically repeat audio until the button is pressed
#define AUTO_REPEAT_AUDIO false

// ====================================================================
// PIN CONFIG
// ====================================================================

// --- TFT ST7789 (VSPI). CS TFT assumed connected directly to GND. ---
#define TFT_SDA 23
#define TFT_SCK 18
#define TFT_DC  2
#define TFT_RST 4
#define TFT_BL  15
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// --- SD Card (HSPI, separated bus from TFT) ---
#define SD_SCK  14
#define SD_MISO 12   // strapping pin - see README.md
#define SD_MOSI 13
#define SD_CS   5

// --- I2S Audio ---
#define I2S_PORT I2S_NUM_0
#define I2S_BCLK 27
#define I2S_LRC  26
#define I2S_DIN  25

// --- Potentiometer volume ---
#define POT_PIN ADC1_CHANNEL_6   // GPIO34

// --- Push button ---
#define BUTTON_PIN 32
#define DEBOUNCE_MS 30

// ====================================================================
// SD CARD PATHS
// ====================================================================

constexpr const char* ROOT_DIR      = "/esp32-multimedia";
constexpr const char* GIF_DIR       = "/esp32-multimedia/gif";
constexpr const char* AUDIO_DIR     = "/esp32-multimedia/audio";
constexpr const char* MANIFEST_PATH = "/esp32-multimedia/manifest.json";

// ====================================================================
// BUFFER SIZES & LIMITS
// ====================================================================

#define MAX_ITEMS      200   // max number of gif+wav pairs in manifest.json
#define AUDIO_BUF_LEN  2048  // chunks of audio data read from SD and sent to I2S
#define CMD_LEN        32    // max length of audio command string sent to audio task
