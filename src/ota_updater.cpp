#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include "ota_updater.h"
#include "config.h"
#include "power_latch.h"
#include "led_patterns.h"
#include "pins.h"
#include "display_events.h"
#include "mega_link.h"

namespace {

// loop() is stuck in ArduinoOTA.handle() for the whole update, so nothing
// else is driving the LED while these patterns run.

bool listening = false;
bool windowOpen = false;
unsigned long windowEndMs = 0;
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

void startListening() {
  if (listening) return;
  ArduinoOTA.begin();
  listening = true;
  Serial.println("OTA listening");
}

}  // namespace

void otaBegin() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }
  ArduinoOTA.onStart([]() {
    Serial.println("OTA update starting");
    // loop() is stuck here until the update ends, so nothing we key can
    // reach VBand -- put the keyer back on its radio meanwhile.
    megaLinkSetVbandReady(false);
    displayOtaStarting();
    pinMode(PIN_LED, OUTPUT);
    progressLedOn = false;
    lastProgressToggleMs = millis();
    setLed(false);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    flickerOtaProgress();
    displayOtaProgress(progress, total); // also keeps the keyer link from timing out
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA update complete");
    displayOtaDone();
    megaLinkSendBye("OTA");
    // ArduinoOTA restarts right after this returns (the uploader has
    // already been sent its OK, so the flash doesn't hold it up).
    powerLatchHoldThroughRestart();
    setLed(false);
    ledFlashSuccess();  // visible before the reboot
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.println("OTA error [" + String(error) + "]");
    displayOtaFailed(); // loop() resumes after this and restores "VB" from vbandIsReady()
    setLed(false);
    flashOtaFailure();
    // Leave time to try again.
    if (windowOpen && windowEndMs - millis() < OTA_WINDOW_MS / 2) windowEndMs = millis() + OTA_WINDOW_MS / 2;
  });
#ifdef OTA_ALWAYS_ON
  startListening();
#endif
}

void otaOpenWindow() {
#ifdef OTA_ALWAYS_ON
  displayOtaAlwaysOn(WiFi.localIP());
#else
  startListening();
  windowOpen = true;
  windowEndMs = millis() + OTA_WINDOW_MS;
  displayOtaWindowOpen(WiFi.localIP(), OTA_WINDOW_MS / 60000);
#endif
}

void otaLoop() {
  if (!listening) return;
  // An update runs entirely inside handle(), so the window can't close on one.
  ArduinoOTA.handle();
  if (windowOpen && (long)(millis() - windowEndMs) >= 0) {
    ArduinoOTA.end();
    listening = false;
    windowOpen = false;
    Serial.println("OTA window closed");
    displayOtaWindowClosed();
  }
}
