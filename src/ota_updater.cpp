#include <Arduino.h>
#include <ArduinoOTA.h>
#include "ota_updater.h"
#include "config.h"
#include "power_latch.h"
#include "led_patterns.h"
#include "pins.h"

namespace {

// loop() is stuck in ArduinoOTA.handle() for the whole update, so nothing
// else is driving the LED while these patterns run.

bool progressLedOn = false;
unsigned long lastProgressToggleMs = 0;

void setLed(bool on) {
  if (powerOffPending()) return;  // power_latch.cpp owns the LED now
  digitalWrite(PIN_LED, on ? HIGH : LOW);
}

// Fast flicker while data is arriving. Driven by onProgress rather than a
// timer, so the LED freezes if the upload stalls.
void flickerOtaProgress() {
  unsigned long now = millis();
  if (now - lastProgressToggleMs < OTA_PROGRESS_FLASH_MS) return;
  lastProgressToggleMs = now;
  progressLedOn = !progressLedOn;
  setLed(progressLedOn);
}

// One long flash so a failed upload looks different from success. The
// board keeps running the old firmware afterward.
void flashOtaFailure() {
  setLed(true);
  delay(OTA_FAILURE_FLASH_MS);
  setLed(false);
}

}  // namespace

void otaBegin() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }
  ArduinoOTA.onStart([]() {
    Serial.println("OTA update starting");
    pinMode(PIN_LED, OUTPUT);
    progressLedOn = false;
    lastProgressToggleMs = millis();
    setLed(false);
  });
  ArduinoOTA.onProgress([](unsigned int, unsigned int) { flickerOtaProgress(); });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA update complete");
    // ArduinoOTA restarts right after this returns (the uploader has
    // already been sent its OK, so the flash doesn't hold it up).
    powerLatchHoldThroughRestart();
    setLed(false);
    ledFlashSuccess();  // visible before the reboot
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.println("OTA error [" + String(error) + "]");
    setLed(false);
    flashOtaFailure();
  });
  ArduinoOTA.begin();
}

void otaLoop() {
  ArduinoOTA.handle();
}
