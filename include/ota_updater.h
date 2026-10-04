#pragma once

// Over-the-air update support (see OTA_HOSTNAME/OTA_PASSWORD in
// config.h). Call otaBegin() once from setup(), otaLoop() every loop().
// Only listens for updates while OTA_ALWAYS_ON is defined in config.h.
// Progress is shown on the keyer's display.
void otaBegin();
void otaLoop();
