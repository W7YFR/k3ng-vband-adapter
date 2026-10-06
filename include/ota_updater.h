#pragma once

// Over-the-air update support (see OTA_HOSTNAME/OTA_PASSWORD in
// config.h). Call otaBegin() once from setup(), otaLoop() every loop().
// Listens per the VB.OTA setting: all the time, not at all, or for
// OTA_WINDOW_MS ("/OTA"). Progress is shown on the keyer's display.
void otaBegin();
void otaLoop();

// OTA_MODE_OFF / _ON / _WINDOW (adapter_settings.h); shows the result on
// the keyer. Called when VB.OTA changes.
void otaSetMode(int mode);

// Opens the OTA_WINDOW_MS window ("/OTA"): sets VB.OTA to 10min.
void otaOpenWindow();
