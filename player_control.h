#pragma once

// Choose a random gif+wav pair from the manifest and start playing both
// (gif loops, audio according to AUTO_REPEAT_AUDIO in config.h).
// Called once in setup() and each time the button is pressed.
void playRandomPair();
