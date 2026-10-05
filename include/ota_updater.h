#pragma once

// Over-the-air update support (see OTA_HOSTNAME/OTA_PASSWORD in
// config.h). Call otaBegin() once from setup(), otaLoop() every loop().
// Listens for updates all the time if OTA_ALWAYS_ON is defined in
// config.h, otherwise only for OTA_WINDOW_MS after otaOpenWindow() ("/OTA"
// keyed on the keyer). Progress is shown on the keyer's display.
void otaBegin();
void otaLoop();

// Starts (or restarts) the OTA_WINDOW_MS window and shows it on the keyer.
void otaOpenWindow();
