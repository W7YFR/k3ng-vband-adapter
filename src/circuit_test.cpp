#include <Arduino.h>
#include "circuit_test.h"
#include "debounced_input.h"
#include "wifi_setup.h"
#include "ota_updater.h"
#include "pins.h"
#include "config.h"

namespace {

DebouncedInput button(PIN_CHANNEL_BUTTON, BUTTON_DEBOUNCE_MS);

}  // namespace

void circuitTestBegin() {
  button.begin(INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  wifiConnect();
  otaBegin();
}

void circuitTestLoop() {
  if (button.update()) {
    digitalWrite(PIN_LED, button.activeNow() ? HIGH : LOW);
  }
  otaLoop();
}
