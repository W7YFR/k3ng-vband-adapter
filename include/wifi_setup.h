#pragma once

// Blocks until WiFi is connected, via WiFiManager's captive-portal
// provisioning flow (see WIFI_MANAGER_AP_NAME in config.h). Restarts the
// device if provisioning fails.
void wifiConnect();

// True if WiFi is currently associated. Reflects live status, not just
// the initial wifiConnect() result -- WiFi can drop later at any time.
bool wifiConnected();
