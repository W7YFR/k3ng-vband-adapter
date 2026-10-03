#include <Arduino.h>
#include "power_latch.h"
#include "pins.h"

void powerLatchBegin() {
  pinMode(PIN_POWER_LATCH, OUTPUT);
  digitalWrite(PIN_POWER_LATCH, HIGH);
}
