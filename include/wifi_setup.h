#pragma once

// Blocks until WiFi is connected, via WiFiManager's captive-portal
// provisioning flow (see WIFI_MANAGER_AP_NAME in config.h). The portal
// opens automatically when there's no usable saved WiFi, or on demand
// when the button is held through boot for PORTAL_HOLD_MS. The LED blinks
// every LED_PORTAL_BLINK_MS while the portal is open. Restarts the device
// if provisioning fails.
void wifiConnect();

// True if WiFi is currently associated. Reflects live status, not just
// the initial wifiConnect() result -- WiFi can drop later at any time.
bool wifiConnected();
