#include <Arduino.h>
#include <driver/gpio.h>
#include "power_latch.h"
#include "debounced_input.h"
#include "mega_link.h"
#include "pins.h"
#include "config.h"

namespace {

// Separate from channel_button.cpp's instance on the same pin: this one is
// polled from its own task, so it keeps working while loop() is stuck in
// a blocking call (e.g. WiFiManager's config portal in wifiConnect()).
DebouncedInput button(PIN_CHANNEL_BUTTON, BUTTON_DEBOUNCE_MS);

volatile bool offPending = false;

// Latch already released; flash the LED until power actually drops (when
// the button is let go). Never returns.
void flashUntilPowerOff() {
  pinMode(PIN_LED, OUTPUT);
  bool on = false;
  for (;;) {
    on = !on;
    digitalWrite(PIN_LED, on ? HIGH : LOW);
    vTaskDelay(pdMS_TO_TICKS(POWER_OFF_FLASH_MS));
  }
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
      offPending = true;
      megaLinkSendBye("OFF"); // power only drops once the button is let go, plenty of time to send
      powerLatchRelease();
      flashUntilPowerOff();
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

bool powerOffPending() {
  return offPending;
}

void powerLatchRelease() {
  gpio_hold_dis((gpio_num_t)PIN_POWER_LATCH);
  digitalWrite(PIN_POWER_LATCH, LOW);
}
