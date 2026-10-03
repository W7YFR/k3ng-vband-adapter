#include <Arduino.h>
#include <WiFiManager.h>
#include "wifi_setup.h"
#include "config.h"

static_assert(sizeof(WIFI_MANAGER_AP_PASSWORD) - 1 >= 8,
              "WIFI_MANAGER_AP_PASSWORD must be at least 8 characters (WPA2)");

void wifiConnect() {
  WiFiManager wm;
  if (!wm.autoConnect(WIFI_MANAGER_AP_NAME, WIFI_MANAGER_AP_PASSWORD)) {
    Serial.println("WiFi provisioning failed, restarting");
    ESP.restart();
  }
  Serial.println("WiFi connected: " + WiFi.localIP().toString());
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}
