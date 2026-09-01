#include <Arduino.h>
#include <WiFiManager.h>
#include "wifi_setup.h"
#include "config.h"

void wifiConnect() {
  WiFiManager wm;
  if (!wm.autoConnect(WIFI_MANAGER_AP_NAME)) {
    Serial.println("WiFi provisioning failed, restarting");
    ESP.restart();
  }
  Serial.println("WiFi connected: " + WiFi.localIP().toString());
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}
