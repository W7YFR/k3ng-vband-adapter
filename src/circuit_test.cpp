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

void sendNextTestScreen() {
  if (!megaLinkUp()) {
    Serial.println("Keyer link down, test screen not sent");
    return;
  }
  const char *screen = TEST_SCREENS[nextTestScreen];
  bool sent = megaLinkSend("ST", screen);
  Serial.printf("Test screen %d %s: %s\n", nextTestScreen + 1, sent ? "sent" : "dropped", screen);
  nextTestScreen = (nextTestScreen + 1) % TEST_SCREEN_COUNT;
}

void updateLed() {
  digitalWrite(PIN_LED, (button.activeNow() || key.activeNow()) ? HIGH : LOW);
}

}  // namespace

void circuitTestBegin() {
  button.begin(INPUT_PULLUP);
  key.begin(INPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  megaLinkBegin(); // before wifiConnect(), which blocks
  wifiConnect();
  otaBegin();
}

void circuitTestLoop() {
  megaLinkLoop();

  if (button.update() && !powerOffPending()) {
    if (button.activeNow()) sendNextTestScreen();
    updateLed();
  }

  if (key.update() && !powerOffPending()) {
    if (key.activeNow()) {
      keyDownMs = millis();
      Serial.println("Key down");
    } else {
      Serial.printf("Key up after %lu ms\n", millis() - keyDownMs);
    }
    updateLed();
  }

  otaLoop();
}
