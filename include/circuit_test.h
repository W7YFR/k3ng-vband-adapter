#pragma once

// Bench test for the hardware around the board (enabled by CIRCUIT_TEST
// in config.h): the soft power latch, the keying line, and the serial
// link to the K3NG keyer. The LED is on while the button is held or the
// key line is active, otherwise blinks slowly until the keyer has been
// heard from; key transitions are logged to Serial. Each
// button press sends the keyer the next of a few test status screens
// (see circuit_test.cpp). Still connects to WiFi and accepts OTA
// updates, but never joins VBand -- it tells the keyer VBand is ready
// anyway, so the keyer switches to the VBand key line for testing.
void circuitTestBegin();
void circuitTestLoop();
