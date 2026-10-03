#include <Arduino.h>
#include <ArduinoOTA.h>
#include "ota_updater.h"
#include "config.h"
#include "power_latch.h"

void otaBegin() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }
  ArduinoOTA.onStart([]() { Serial.println("OTA update starting"); });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA update complete");
    // ArduinoOTA restarts right after this returns.
    powerLatchHoldThroughRestart();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.println("OTA error [" + String(error) + "]");
  });
  ArduinoOTA.begin();
}

void otaLoop() {
  ArduinoOTA.handle();
}
