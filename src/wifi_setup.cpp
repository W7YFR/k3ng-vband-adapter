#include <Arduino.h>
#include <Ticker.h>
#include <WiFiManager.h>
#include "wifi_setup.h"
#include "vband_settings.h"
#include "power_latch.h"
#include "pins.h"
#include "config.h"

static_assert(sizeof(WIFI_MANAGER_AP_PASSWORD) - 1 >= 8,
              "WIFI_MANAGER_AP_PASSWORD must be at least 8 characters (WPA2)");

namespace {

// loop() doesn't run while the portal blocks, so the portal's LED blink
// runs off a timer instead of led_indicator.cpp.
Ticker portalBlink;
bool portalLedOn = false;

void togglePortalLed() {
  if (powerOffPending()) return;  // power_latch.cpp owns the LED now
  portalLedOn = !portalLedOn;
  digitalWrite(PIN_LED, portalLedOn ? HIGH : LOW);
}

void startPortalBlink() {
  pinMode(PIN_LED, OUTPUT);
  portalBlink.attach_ms(LED_PORTAL_BLINK_MS, togglePortalLed);
}

void stopPortalBlink() {
  portalBlink.detach();
  portalLedOn = false;
  if (!powerOffPending()) digitalWrite(PIN_LED, LOW);
}

// True if the button (still held from powering on) stays held until
// PORTAL_HOLD_MS after boot. Returns as soon as it's released, so a
// normal press-and-let-go boot isn't delayed.
bool portalRequestedAtBoot() {
  pinMode(PIN_CHANNEL_BUTTON, INPUT_PULLUP);
  while (millis() < PORTAL_HOLD_MS) {
    if (digitalRead(PIN_CHANNEL_BUTTON) == HIGH) return false;
    delay(10);
  }
  return digitalRead(PIN_CHANNEL_BUTTON) == LOW;
}

}  // namespace

void wifiConnect() {
  WiFiManager wm;

  // Shown on the portal's WiFi page, prefilled with the current values;
  // saved along with the WiFi credentials.
  WiFiManagerParameter nameParam("vband_name", "VBand user name",
                                 vbandSettingsName().c_str(), VBAND_SETTING_MAX_LEN);
  WiFiManagerParameter roomParam("vband_room", "VBand custom room",
                                 vbandSettingsRoom().c_str(), VBAND_SETTING_MAX_LEN);
  wm.addParameter(&nameParam);
  wm.addParameter(&roomParam);
  wm.setSaveParamsCallback([&]() {
    vbandSettingsSave(nameParam.getValue(), roomParam.getValue());
  });
  wm.setAPCallback([](WiFiManager *) { startPortalBlink(); });

  bool connected = false;
  if (portalRequestedAtBoot()) {
    Serial.println("Button held at boot, opening config portal");
    connected = wm.startConfigPortal(WIFI_MANAGER_AP_NAME, WIFI_MANAGER_AP_PASSWORD);
    // Exited without saving: carry on with whatever WiFi is already saved.
  }
  if (!connected) {
    connected = wm.autoConnect(WIFI_MANAGER_AP_NAME, WIFI_MANAGER_AP_PASSWORD);
  }
  stopPortalBlink();

  if (!connected) {
    Serial.println("WiFi provisioning failed, restarting");
    ESP.restart();
  }
  Serial.println("WiFi connected: " + WiFi.localIP().toString());
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}
