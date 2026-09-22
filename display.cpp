#include "display.h"
#include "globals.h"
#include "config.h"

void writeCommand(uint8_t cmd) {
  digitalWrite(TFT_DC, LOW);
  tftSPI.transfer(cmd);
}

void writeData(uint8_t data) {
  digitalWrite(TFT_DC, HIGH);
  tftSPI.transfer(data);
}

void writeData16(uint16_t data) {
  digitalWrite(TFT_DC, HIGH);
  tftSPI.transfer(data >> 8);
  tftSPI.transfer(data & 0xFF);
}

void setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  writeCommand(0x2A);
  writeData16(x0);
  writeData16(x1);
  writeCommand(0x2B);
  writeData16(y0);
  writeData16(y1);
  writeCommand(0x2C);
}

void fillScreen(uint16_t color) {
  setAddressWindow(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);
  digitalWrite(TFT_DC, HIGH);
  for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++) {
    tftSPI.transfer(color >> 8);
    tftSPI.transfer(color & 0xFF);
  }
}

static void resetDisplay() {
  digitalWrite(TFT_RST, LOW);
  delay(100);
  digitalWrite(TFT_RST, HIGH);
  delay(150);
}

static void initST7789() {
  writeCommand(0x01);   // software reset
  delay(150);
  writeCommand(0x11);   // sleep out
  delay(120);
  writeCommand(0x3A);   // pixel format
  writeData(0x55);      // 16-bit/pixel
  writeCommand(0x36);   // memory access control
  writeData(0x00);
  writeCommand(0x21);   // display inversion on
  writeCommand(0x13);   // normal display mode
  writeCommand(0x29);   // display on
  delay(100);
}

void tftInit() {
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tftSPI.begin(TFT_SCK, -1, TFT_SDA, -1);
  tftSPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE3));

  resetDisplay();
  initST7789();
  fillScreen(0x0000);
}
