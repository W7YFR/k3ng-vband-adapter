#include <Arduino.h>
#include "circuit_test.h"
#include "debounced_input.h"
#include "wifi_setup.h"
#include "ota_updater.h"
#include "power_latch.h"
#include "mega_link.h"
#include "pins.h"
#include "config.h"

namespace {

DebouncedInput button(PIN_CHANNEL_BUTTON, BUTTON_DEBOUNCE_MS);
DebouncedInput key(PIN_KEY, DEBOUNCE_MS, KEY_ACTIVE_HIGH);

unsigned long keyDownMs = 0;

// With the LED otherwise idle, it blinks at this on/off interval while
// nothing has been heard from the keyer, so the link's keyer -> ESP32
// direction can be checked without a serial monitor.
const unsigned long LINK_DOWN_BLINK_MS = 500;

// Status screens sent to the keyer, one per button press, in turn. Each
// checks something different about how it lays out "ST" frames on its
// 18 x 4 display: all four rows, centering, a row exactly the display's
// width and one too long for it, and a realistic room-status screen.
const char *TEST_SCREENS[] = {
    "4000,LINK TEST|row two|row three|row four",
    "4000,Centered",
    "4000,123456789012345678|ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "4000,CH: Channel 1|3 in room|W7YFR K1ABC|N0CALL",
};
const int TEST_SCREEN_COUNT = sizeof(TEST_SCREENS) / sizeof(TEST_SCREENS[0]);
int nextTestScreen = 0;

// Sent even while we haven't heard from the keyer: the keyer only needs
// the ESP32 -> keyer direction to show it.
void sendNextTestScreen() {
  const char *screen = TEST_SCREENS[nextTestScreen];
  bool sent = megaLinkSend("ST", screen, true);
  Serial.printf("Test screen %d %s (link %s): %s\n", nextTestScreen + 1,
                sent ? "sent" : "dropped", megaLinkUp() ? "up" : "down", screen);
  nextTestScreen = (nextTestScreen + 1) % TEST_SCREEN_COUNT;
}

// On while the button is held or the key line is active; otherwise a
// slow blink while the keyer hasn't been heard from, off once it has.
void updateLed() {
  bool on;
  if (button.activeNow() || key.activeNow()) {
    on = true;
  } else if (!megaLinkUp()) {
    on = (millis() / LINK_DOWN_BLINK_MS) % 2;
  } else {
    on = false;
  }
  digitalWrite(PIN_LED, on ? HIGH : LOW);
}

}  // namespace

void circuitTestBegin() {
  button.begin(INPUT_PULLUP);
  key.begin(INPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  megaLinkBegin(); // before wifiConnect(), which blocks
  // Never joins VBand, but claims it's ready so the keyer moves keying to
  // the VBand key line and that line can be tested.
  megaLinkSetVbandReady(true);
  wifiConnect();
  otaBegin();
}

void circuitTestLoop() {
  megaLinkLoop();

  if (button.update() && button.activeNow() && !powerOffPending()) {
    sendNextTestScreen();
  }

  if (key.update()) {
    if (key.activeNow()) {
      keyDownMs = millis();
      Serial.println("Key down");
    } else {
      Serial.printf("Key up after %lu ms\n", millis() - keyDownMs);
    }
  }

  if (!powerOffPending()) updateLed(); // power_latch.cpp flashes the LED itself once powering off

  otaLoop();
}
