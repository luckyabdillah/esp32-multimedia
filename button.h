#pragma once

// Set pinMode button. Call once in setup().
void buttonInit();

// Read button state and detect short press (with debounce).
// Call each loop() iteration. Short press effect: randomize
// new pair via playRandomPair().
void handleButton();
