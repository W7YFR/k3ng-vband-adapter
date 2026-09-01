#include <Arduino.h>
#include "keyer.h"
#include "debounced_input.h"
#include "pins.h"
#include "config.h"

namespace {

// Keying line has its own external divider biasing it, so plain INPUT --
// an internal pull-up here would skew the divider math.
DebouncedInput key(PIN_KEY, DEBOUNCE_MS);
unsigned long downStartMs = 0;
unsigned long lastReleaseMs = 0;

}  // namespace

void keyerBegin() {
  key.begin(INPUT);
}

void keyerLoop(KeyerSpaceMarkCallback onSpaceMark) {
  if (!key.update()) return;

  unsigned long now = millis();
  if (key.activeNow()) {
    downStartMs = now;
  } else {
    unsigned long space = min(downStartMs - lastReleaseMs, (unsigned long)MAX_TIME_MS);
    unsigned long mark = min(now - downStartMs, (unsigned long)MAX_TIME_MS);
    lastReleaseMs = now;
    onSpaceMark(space, mark);
  }
}
