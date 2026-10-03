#pragma once

// Bench test for the soft power latch circuit (enabled by CIRCUIT_TEST in
// config.h): mirrors PIN_CHANNEL_BUTTON onto PIN_LED -- on while the
// button is held, off when released. Still connects to WiFi and accepts
// OTA updates, but never joins VBand.
void circuitTestBegin();
void circuitTestLoop();
