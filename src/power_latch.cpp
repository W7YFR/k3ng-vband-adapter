#include <Arduino.h>
#include <driver/gpio.h>
#include "power_latch.h"
#include "debounced_input.h"
#include "mega_link.h"
#include "display_events.h"
#include "pins.h"
#include "config.h"

namespace {

// Separate from channel_button.cpp's instance on the same pin: this one is
// polled from its own task, so it keeps working while loop() is stuck in
// a blocking call (e.g. WiFiManager's config portal in wifiConnect()).
DebouncedInput button(PIN_CHANNEL_BUTTON, BUTTON_DEBOUNCE_MS);

volatile bool offPending = false;

// How long after the button is let go power should be gone. Still running
// after that, something else is powering the board (USB).
constexpr unsigned long POWER_OFF_CONFIRM_MS = 1500;

// Tells the keyer, releases the latch, and flashes the LED until power
// drops. The held button keeps the board powered through D1, so that's
// only once it's let go. Returns if the power doesn't drop: then it
// re-latches, hands the LED back, and says so on the keyer.
void powerDown() {
  offPending = true;
  megaLinkSendBye("OFF");
  vTaskDelay(pdMS_TO_TICKS(20)); // on battery, power goes the moment the latch drops; let the BYE out first
  powerLatchRelease();

  pinMode(PIN_LED, OUTPUT);
  bool on = false;
  unsigned long releasedAtMs = millis();
  for (;;) {
    on = !on;
    digitalWrite(PIN_LED, on ? HIGH : LOW);
    vTaskDelay(pdMS_TO_TICKS(POWER_OFF_FLASH_MS));
    if (digitalRead(PIN_CHANNEL_BUTTON) == LOW) {
      releasedAtMs = millis(); // still held
    } else if (millis() - releasedAtMs >= POWER_OFF_CONFIRM_MS) {
      break;
    }
  }

  digitalWrite(PIN_POWER_LATCH, HIGH);
  digitalWrite(PIN_LED, LOW);
  offPending = false;
  Serial.println("Power off: still powered (USB?), staying on");
  displayPowerOffFailed();
}

void powerOffButtonTask(void*) {
  // The press that powered the board on is usually still held at boot;
  // don't count it until the button has been released once.
  bool armed = digitalRead(PIN_CHANNEL_BUTTON) == HIGH;
  unsigned long pressedAtMs = 0;

  for (;;) {
    if (button.update()) {
      if (!button.activeNow()) {
        armed = true;
      } else if (armed) {
        pressedAtMs = millis();
      }
    }
    if (armed && button.activeNow() &&
        millis() - pressedAtMs >= POWER_OFF_HOLD_MS) {
      Serial.println("Power off: release button");
      powerDown();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

}  // namespace

void powerLatchBegin() {
  pinMode(PIN_POWER_LATCH, OUTPUT);
  digitalWrite(PIN_POWER_LATCH, HIGH);
  // If we're coming back from powerLatchHoldThroughRestart(), the pad is
  // still locked HIGH. Release it only now that the output register is
  // HIGH too, so the line never glitches low.
  gpio_hold_dis((gpio_num_t)PIN_POWER_LATCH);
}

void powerOffButtonBegin() {
  button.begin(INPUT_PULLUP);
  xTaskCreate(powerOffButtonTask, "power_off_btn", 2048, nullptr, 1, nullptr);
}

void powerLatchHoldThroughRestart() {
  digitalWrite(PIN_POWER_LATCH, HIGH);
  gpio_hold_en((gpio_num_t)PIN_POWER_LATCH);
}

void powerOffNow() {
  Serial.println("Power off (command)");
  powerDown();
}

bool powerOffPending() {
  return offPending;
}

void powerLatchRelease() {
  gpio_hold_dis((gpio_num_t)PIN_POWER_LATCH);
  digitalWrite(PIN_POWER_LATCH, LOW);
}
