#include <Arduino.h>
#include <Ticker.h>
#include <WiFiManager.h>
#include "wifi_setup.h"
#include "vband_settings.h"
#include "power_latch.h"
#include "led_patterns.h"
#include "display_events.h"
#include "pins.h"
#include "config.h"

static_assert(sizeof(WIFI_MANAGER_AP_PASSWORD) - 1 >= 8,
              "WIFI_MANAGER_AP_PASSWORD must be at least 8 characters (WPA2)");

namespace {

// loop() doesn't run while wifiConnect() blocks, so its LED blinks
// (connecting, portal open) run off a timer instead of led_indicator.cpp.
Ticker blinkTicker;
bool blinkLedOn = false;

void toggleBlinkLed() {
  if (powerOffPending()) return;  // power_latch.cpp owns the LED now
  blinkLedOn = !blinkLedOn;
  digitalWrite(PIN_LED, blinkLedOn ? HIGH : LOW);
}

// Starts (or switches to) blinking at intervalMs, from LED off.
void startBlink(unsigned long intervalMs) {
  blinkTicker.detach();
  pinMode(PIN_LED, OUTPUT);
  blinkLedOn = false;
  if (!powerOffPending()) digitalWrite(PIN_LED, LOW);
  blinkTicker.attach_ms(intervalMs, toggleBlinkLed);
}

void stopBlink() {
  blinkTicker.detach();
  blinkLedOn = false;
  if (!powerOffPending()) digitalWrite(PIN_LED, LOW);
}

// True if the button (still held from powering on) stays held until
// PORTAL_HOLD_MS after boot. Returns as soon as it's released, so a
// normal press-and-let-go boot isn't delayed. Lights the LED while the
// power-on press is held, as a "booted" signal; on release the connecting
// blink takes over, or the portal blink if the hold reaches PORTAL_HOLD_MS.
bool portalRequestedAtBoot() {
  pinMode(PIN_CHANNEL_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  if (digitalRead(PIN_CHANNEL_BUTTON) == HIGH) return false;

  digitalWrite(PIN_LED, HIGH);
  while (millis() < PORTAL_HOLD_MS) {
    if (digitalRead(PIN_CHANNEL_BUTTON) == HIGH) {
      digitalWrite(PIN_LED, LOW);
      return false;
    }
    delay(10);
  }
  return true;
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
  wm.setAPCallback([](WiFiManager *) {
    startBlink(LED_PORTAL_BLINK_MS);
    displayWifiPortal();
  });

  bool connected = false;
  if (portalRequestedAtBoot()) {
    Serial.println("Button held at boot, opening config portal");
    connected = wm.startConfigPortal(WIFI_MANAGER_AP_NAME, WIFI_MANAGER_AP_PASSWORD);
    // Exited without saving: carry on with whatever WiFi is already saved.
  }
  if (!connected) {
    // Switches to the portal blink via the AP callback if it can't connect.
    startBlink(LED_WIFI_CONNECTING_BLINK_MS);
    displayWifiConnecting();
    connected = wm.autoConnect(WIFI_MANAGER_AP_NAME, WIFI_MANAGER_AP_PASSWORD);
  }
  stopBlink();

  if (!connected) {
    Serial.println("WiFi provisioning failed, restarting");
    ESP.restart();
  }
  Serial.println("WiFi connected: " + WiFi.localIP().toString());
  displayWifiConnected(WiFi.localIP());
  ledFlashSuccess();
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}
