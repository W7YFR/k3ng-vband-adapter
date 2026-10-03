#pragma once

// Normal operation: connects to WiFi, joins VBand, and runs the keyer,
// channel button, status LED, sidetone, and OTA updates. Used whenever
// CIRCUIT_TEST in config.h is not defined.
void vbandAppBegin();
void vbandAppLoop();
