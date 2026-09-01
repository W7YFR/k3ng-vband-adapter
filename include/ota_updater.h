#pragma once

// Over-the-air update support (see OTA_HOSTNAME/OTA_PASSWORD in
// config.h). Call otaBegin() once from setup(), otaLoop() every loop().
void otaBegin();
void otaLoop();
