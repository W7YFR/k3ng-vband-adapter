#include <Arduino.h>
#include <driver/gpio.h>
#include "power_latch.h"
#include "pins.h"

void powerLatchBegin() {
  pinMode(PIN_POWER_LATCH, OUTPUT);
  digitalWrite(PIN_POWER_LATCH, HIGH);
  // If we're coming back from powerLatchHoldThroughRestart(), the pad is
  // still locked HIGH. Release it only now that the output register is
  // HIGH too, so the line never glitches low.
  gpio_hold_dis((gpio_num_t)PIN_POWER_LATCH);
}

void powerLatchHoldThroughRestart() {
  digitalWrite(PIN_POWER_LATCH, HIGH);
  gpio_hold_en((gpio_num_t)PIN_POWER_LATCH);
}
