#include <Arduino.h>
#include "led_patterns.h"
#include "power_latch.h"
#include "pins.h"
#include "config.h"

void ledFlashSuccess() {
  pinMode(PIN_LED, OUTPUT);
  for (int i = 0; i < LED_SUCCESS_FLASH_COUNT; i++) {
    if (powerOffPending()) return;
    digitalWrite(PIN_LED, HIGH);
    delay(LED_SUCCESS_FLASH_MS);
    if (powerOffPending()) return;
    digitalWrite(PIN_LED, LOW);
    delay(LED_SUCCESS_FLASH_MS);
  }
}
