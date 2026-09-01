#include <Arduino.h>
#include "channel_button.h"
#include "debounced_input.h"
#include "pins.h"
#include "config.h"

namespace {

DebouncedInput button(PIN_CHANNEL_BUTTON, BUTTON_DEBOUNCE_MS);

}  // namespace

void channelButtonBegin() {
  button.begin(INPUT_PULLUP);
}

void channelButtonLoop(ChannelButtonPressCallback onPress) {
  if (button.update() && button.activeNow()) {
    onPress();
  }
}
