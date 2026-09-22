#include "gif_player.h"
#include "globals.h"
#include "config.h"
#include "display.h"

// ====================================================================
// GIF DRAW - draw one line of decoded GIF to TFT. Handles transparency
// by splitting the line into multiple "runs" of non-transparent pixels
// so that transparent areas do not overwrite existing pixels on the
// screen (this fixes the case of delta-encoded GIFs).
// ====================================================================

static void GIFDraw(GIFDRAW* pDraw) {
  int x = pDraw->iX;
  int y = pDraw->iY + pDraw->y;
  int w = pDraw->iWidth;

  if (x < 0 || y < 0 || x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
  if (x + w > TFT_WIDTH) w = TFT_WIDTH - x;
  if (w <= 0) return;

  uint8_t*  pixels  = pDraw->pPixels;
  uint16_t* palette = pDraw->pPalette;

  if (pDraw->ucHasTransparency) {
    int start = 0;
    while (start < w) {
      while (start < w && pixels[start] == pDraw->ucTransparent) start++;
      if (start >= w) break;

      int runStart = start;
      while (start < w && pixels[start] != pDraw->ucTransparent) start++;
      int runLength = start - runStart;
      if (runLength <= 0) continue;

      setAddressWindow(x + runStart, y, x + start - 1, y);
      digitalWrite(TFT_DC, HIGH);

      for (int i = 0; i < runLength; i++) {
        uint16_t color = palette[pixels[runStart + i]];
        tftSPI.transfer(color >> 8);
        tftSPI.transfer(color & 0xFF);
      }
    }
    return;
  }

  setAddressWindow(x, y, x + w - 1, y);
  digitalWrite(TFT_DC, HIGH);
  for (int i = 0; i < w; i++) {
    uint16_t color = palette[pixels[i]];
    tftSPI.transfer(color >> 8);
    tftSPI.transfer(color & 0xFF);
  }
}

// ====================================================================
// FILE CALLBACKS - read GIF directly from SD, protected by sdMutex
// because SD is shared with audio task on another core.
// ====================================================================

static void* GIFOpenFile(const char* fname, int32_t* pSize) {
  if (xSemaphoreTake(sdMutex, portMAX_DELAY) != pdTRUE) {
    *pSize = 0;
    return nullptr;
  }

  if (gifFile) gifFile.close();
  gifFile = SD.open(currentGifPath.c_str(), FILE_READ);

  if (!gifFile) {
    Serial.printf("GIFOpenFile: failed to open %s\n", currentGifPath.c_str());
    *pSize = 0;
    xSemaphoreGive(sdMutex);
    return nullptr;
  }

  *pSize = (int32_t)gifFile.size();

  xSemaphoreGive(sdMutex);
  return &gifFile;
}

static void GIFCloseFile(void* pHandle) {
  if (xSemaphoreTake(sdMutex, portMAX_DELAY) != pdTRUE) return;
  if (gifFile) gifFile.close();
  xSemaphoreGive(sdMutex);
}

static int32_t GIFReadFile(GIFFILE* pFile, uint8_t* pBuf, int32_t iLen) {
  if (!pBuf || iLen <= 0) return 0;

  if (xSemaphoreTake(sdMutex, portMAX_DELAY) != pdTRUE) return 0;

  if (!gifFile) {
    xSemaphoreGive(sdMutex);
    return 0;
  }

  if ((int32_t)gifFile.position() != pFile->iPos) {
    if (!gifFile.seek(pFile->iPos)) {
      xSemaphoreGive(sdMutex);
      return 0;
    }
  }

  int32_t remaining = pFile->iSize - pFile->iPos;
  if (remaining <= 0) {
    xSemaphoreGive(sdMutex);
    return 0;
  }

  if (iLen > remaining) iLen = remaining;

  int32_t bytesRead = gifFile.read(pBuf, iLen);
  if (bytesRead > 0) {
    pFile->iPos += bytesRead;
  }

  xSemaphoreGive(sdMutex);
  return (bytesRead > 0) ? bytesRead : 0;
}

static int32_t GIFSeekFile(GIFFILE* pFile, int32_t iPosition) {
  if (xSemaphoreTake(sdMutex, portMAX_DELAY) != pdTRUE) return -1;

  if (!gifFile) {
    xSemaphoreGive(sdMutex);
    return -1;
  }

  if (iPosition < 0) iPosition = 0;
  if (iPosition > pFile->iSize) iPosition = pFile->iSize;

  if (!gifFile.seek(iPosition)) {
    xSemaphoreGive(sdMutex);
    return -1;
  }

  pFile->iPos = iPosition;

  xSemaphoreGive(sdMutex);
  return pFile->iPos;
}

// ====================================================================
// PUBLIC INTERFACE - called from esp32-multimedia.ino / player_control.cpp
// ====================================================================

void setGif(const String &basename) {
  if (basename.length() == 0) {
    Serial.println("No GIF available to select");
    return;
  }
  g_currentBasename = basename;
  g_gifNeedsReload = true;
}

void reopenGifIfNeeded() {
  if (!g_gifNeedsReload) return;
  g_gifNeedsReload = false;

  if (gifEverOpened) gif.close();

  currentGifPath = String(GIF_DIR) + "/" + g_currentBasename + ".gif";
  gif.begin(LITTLE_ENDIAN_PIXELS);

  if (!gif.open("", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    Serial.printf("Failed to open GIF %s (err %d)\n",
        currentGifPath.c_str(), gif.getLastError());
    gifReady = false;
    fillScreen(0xF800);
    return;
  }

  gifEverOpened = true;
  Serial.printf("GIF loaded: %s (%dx%d)\n",
      currentGifPath.c_str(), gif.getCanvasWidth(), gif.getCanvasHeight());

  fillScreen(0x0000);
  gifReady = true;
}
