#include <Arduino.h>
#include "config.h"
#include "power_latch.h"
#include "vband_settings.h"
#include "adapter_settings.h"
#include "vband_app.h"
#include "circuit_test.h"

void setup() {
  // Do not run any code before powerLatchBegin().
  // It is responsible for keeping the board on so
  // the button does not need to be held.
  powerLatchBegin();

  Serial.begin(115200);
  powerOffButtonBegin();
  vbandSettingsBegin();
  adapterSettingsBegin();
#ifdef CIRCUIT_TEST
  circuitTestBegin();
#else
  vbandAppBegin();
#endif
}

void loop() {
#ifdef CIRCUIT_TEST
  circuitTestLoop();
#else
  vbandAppLoop();
#endif
}
