#pragma once

#include <Arduino.h>

// Setup I2S, configure potentiometer ADC, create command queue, and
// spawn audio task on core 0. Call once in setup().
void audioInit();

// Send a command to the audio task to play the given basename. loopIt=true
// means the audio will loop until a new command arrives; false means
// play once then stop. Safe to call from core 1 (main loop/button).
void sendAudioCommand(const String &basename, bool loopIt);
