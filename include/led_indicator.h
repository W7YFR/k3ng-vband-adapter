#pragma once

// Non-blocking status LED on PIN_LED, reflecting WiFi/VBand connection
// state:
//   - WiFi disconnected: blink at LED_WIFI_DISCONNECTED_BLINK_MS on/off.
//   - WiFi connected, connecting to VBand or joining a room: blink at
//     LED_WIFI_CONNECTED_BLINK_MS on/off. In the lobby, or off VBand on
//     purpose (VB.START off, /DIS): off.
//   - Joined: off (or following your keying, the VB.LED setting),
//     except immediately after each join confirmation (including channel
//     switches), when it plays a one-shot Morse flash at MORSE_WPM of
//     vbandChannelCode() first.
void ledBegin();
void ledLoop();
