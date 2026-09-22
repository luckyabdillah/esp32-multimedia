#pragma once

#include <Arduino.h>

// Full initialization: pin modes, start SPI, reset panel, send
// ST7789 init commands, and clear screen to black.
// Call once in setup(), before SD/audio is initialized.
void tftInit();

// Fill entire screen with a single RGB565 color.
// Call after tftInit() and before drawing anything else.
void fillScreen(uint16_t color);

// Low-level primitive, also used by gif_player.cpp when
// drawing decoded GIF frames.
void writeCommand(uint8_t cmd);
void writeData(uint8_t data);
void writeData16(uint16_t data);
void setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
