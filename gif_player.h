#pragma once

#include <Arduino.h>

// Set of GIF basename to play (e.g. "007" -> /gif/007.gif) and mark
// it so that loop() will reopen the GIF file on the next iteration.
void setGif(const String &basename);

// Called every loop iteration. If there is a request to change the GIF
// (from setGif), this function will actually open the file on the SD.
// Does nothing if there are no changes.
void reopenGifIfNeeded();
