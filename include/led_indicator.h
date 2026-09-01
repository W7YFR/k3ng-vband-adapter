#pragma once

// Non-blocking status LED on PIN_LED, reflecting WiFi/VBand connection
// state:
//   - WiFi disconnected: blink at LED_WIFI_DISCONNECTED_BLINK_MS on/off.
//   - WiFi connected, not yet joined to a room: blink at
//     LED_WIFI_CONNECTED_BLINK_MS on/off.
//   - Joined: off, except immediately after each join confirmation
//     (including channel switches), when it plays a one-shot Morse
//     flash at MORSE_WPM of vbandChannelCode() before returning to off.
void ledBegin();
void ledLoop();
