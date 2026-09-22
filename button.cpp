#include "button.h"
#include "config.h"
#include "player_control.h"

static bool buttonDown = false;
static unsigned long buttonDownAt = 0;

void buttonInit() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void handleButton() {
  bool pressedNow = (digitalRead(BUTTON_PIN) == LOW);
  unsigned long now = millis();

  if (pressedNow && !buttonDown) {
    buttonDown = true;
    buttonDownAt = now;
  } else if (!pressedNow && buttonDown) {
    buttonDown = false;
    unsigned long heldMs = now - buttonDownAt;
    if (heldMs >= DEBOUNCE_MS) {
      playRandomPair();
    }
  }
}
