#include <Arduino.h>
#include <Ticker.h>
#include <WiFiManager.h>
#include <esp_wifi.h>
#include "wifi_setup.h"
#include "vband_settings.h"
#include "power_latch.h"
#include "led_patterns.h"
#include "display_events.h"
#include "mega_link.h"
#include "adapter_settings.h"
#include "pins.h"
#include "config.h"
#include "log.h"

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

// Set by wifiRestartIntoPortal(); RTC memory survives a software restart
// but not a power cycle, and isn't cleared at boot.
constexpr uint32_t PORTAL_REQUEST_MAGIC = 0x50525441;
RTC_NOINIT_ATTR uint32_t portalRequest;

bool portalRequestedByCommand() {
  bool requested = portalRequest == PORTAL_REQUEST_MAGIC;
  portalRequest = 0;
  return requested;
}

void setPhy(int phy) {
  uint8_t protocols = phy == WIFI_PHY_B    ? WIFI_PROTOCOL_11B
                      : phy == WIFI_PHY_BG ? (WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G)
                                           : (WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  if (esp_wifi_set_protocol(WIFI_IF_STA, protocols) != ESP_OK) Log.println("WiFi mode not set");
}

// With VB.PHY limiting the modes, connect to the saved network ourselves
// first: if the access point won't take us in that mode, go back to b/g/n
// rather than fall into the config portal, where the keyer can't reach the
// setting to change it back.
bool connectInPhyMode() {
  WiFi.begin(); // the saved network
  unsigned long startMs = millis();
  while (millis() - startMs < WIFI_PHY_CONNECT_WAIT_MS) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(100);
  }
  Log.println("WiFi didn't connect in the chosen mode; using b/g/n");
  WiFi.disconnect();
  setPhy(WIFI_PHY_BGN);
  return false;
}

}  // namespace

void wifiRestartIntoPortal() {
  Log.println("Restarting into the config portal");
  displayPortalRestart();
  megaLinkSendBye("AP");
  delay(100); // let the BYE and the screen out of the UART
  portalRequest = PORTAL_REQUEST_MAGIC;
  powerLatchHoldThroughRestart();
  ESP.restart();
}

void wifiConnect() {
  WiFiManager wm;

  // VB.PHY: leaving out 802.11n (or g too) keeps the radio to slower rates,
  // which get through a busy 2.4 GHz channel more often; VBand's traffic is
  // tiny, so the speed costs nothing. Set before connecting.
  WiFi.mode(WIFI_STA);
  int phy = adapterSetting(AdapterSetting::Phy);
  setPhy(phy);

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
  if (portalRequestedByCommand() || portalRequestedAtBoot()) {
    Log.println("Opening config portal");
    connected = wm.startConfigPortal(WIFI_MANAGER_AP_NAME, WIFI_MANAGER_AP_PASSWORD);
    // Exited without saving: carry on with whatever WiFi is already saved.
  }
  if (!connected) {
    // Switches to the portal blink via the AP callback if it can't connect.
    startBlink(LED_WIFI_CONNECTING_BLINK_MS);
    displayWifiConnecting();
    if (phy != WIFI_PHY_BGN) connected = connectInPhyMode();
    if (!connected) connected = wm.autoConnect(WIFI_MANAGER_AP_NAME, WIFI_MANAGER_AP_PASSWORD);
  }
  stopBlink();

  if (!connected) {
    Log.println("WiFi provisioning failed, restarting");
    ESP.restart();
  }
  // Modem sleep (the ESP32 default) dozes between access point beacons;
  // received packets get delayed or lost, and the lost ones stall the
  // VBand connection for seconds while TCP backs off and retransmits.
  WiFi.setSleep(false);
  int8_t power = 0;
  esp_wifi_get_max_tx_power(&power); // in 0.25 dBm
  uint8_t used = 0;
  esp_wifi_get_protocol(WIFI_IF_STA, &used);
  Log.printf("WiFi connected: %s, channel %d, tx power %.1f dBm, modes %s%s%s\n", WiFi.localIP().toString().c_str(),
             WiFi.channel(), power / 4.0, used & WIFI_PROTOCOL_11B ? "b" : "", used & WIFI_PROTOCOL_11G ? "g" : "",
             used & WIFI_PROTOCOL_11N ? "n" : "");
  displayWifiConnected(WiFi.localIP());
  ledFlashSuccess();
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}
