#include <Arduino.h>
#include <WiFiManager.h>
#include "wifi_setup.h"
#include "vband_settings.h"
#include "config.h"

static_assert(sizeof(WIFI_MANAGER_AP_PASSWORD) - 1 >= 8,
              "WIFI_MANAGER_AP_PASSWORD must be at least 8 characters (WPA2)");

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

  if (!wm.autoConnect(WIFI_MANAGER_AP_NAME, WIFI_MANAGER_AP_PASSWORD)) {
    Serial.println("WiFi provisioning failed, restarting");
    ESP.restart();
  }
  Serial.println("WiFi connected: " + WiFi.localIP().toString());
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}
