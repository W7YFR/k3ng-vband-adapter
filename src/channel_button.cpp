#include <Arduino.h>
#include "channel_button.h"
#include "debounced_input.h"
#include "pins.h"
#include "config.h"

namespace {

DebouncedInput button(PIN_CHANNEL_BUTTON, BUTTON_DEBOUNCE_MS);

// The press that powered the board on may still be held when this starts
// watching; only a press that began after a release counts.
bool armed = false;
bool pressed = false;
unsigned long pressedAtMs = 0;

}  // namespace

void channelButtonBegin() {
  button.begin(INPUT_PULLUP);
  armed = digitalRead(PIN_CHANNEL_BUTTON) == HIGH;
}

void channelButtonLoop(ChannelButtonPressCallback onPress) {
  if (!button.update()) return;
  if (button.activeNow()) {
    pressed = armed;
    pressedAtMs = millis();
    return;
  }
  armed = true;
  // A hold long enough to power off (power_latch.cpp) isn't a channel
  // change, even if power stays up (e.g. on USB).
  if (pressed && millis() - pressedAtMs < POWER_OFF_HOLD_MS) onPress();
  pressed = false;
}
