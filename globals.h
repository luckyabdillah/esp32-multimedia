#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <AnimatedGIF.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// ====================================================================
// SHARED DATA TYPES
// ====================================================================

struct WAVInfo {
  uint16_t audioFormat;
  uint16_t channels;
  uint32_t sampleRate;
  uint16_t bitsPerSample;
  uint32_t dataSize;
};

// ====================================================================
// HARDWARE OBJECTS / LIBRARY (defined in globals.cpp)
// ====================================================================

extern SPIClass tftSPI;
extern SPIClass sdSPI;
extern AnimatedGIF gif;

// ====================================================================
// MANIFEST (list of gif+wav pairs from SD scan)
// ====================================================================

extern String manifestItems[];
extern int    manifestCount;

// ====================================================================
// STATE PLAYBACK GIF (used by gif_player & player_control)
// ====================================================================

extern String g_currentBasename;
extern bool   g_gifNeedsReload;
extern bool   gifReady;
extern bool   gifEverOpened;

extern File   gifFile;
extern String currentGifPath;

// ====================================================================
// COMMUNICATION BETWEEN CORES (main/core1 <-> audio task/core0)
// ====================================================================

extern QueueHandle_t     audioCmdQueue;
extern SemaphoreHandle_t sdMutex;
