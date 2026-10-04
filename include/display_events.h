#pragma once

#include <Arduino.h>

// Status screens for the K3NG keyer's display, sent as "ST" frames over
// mega_link. Each is up to four short rows, which the keyer centers and
// cuts off past its width (18 characters on the w7yfr OLED). The keyer
// adds its own "TX n" on the bottom row when it switches key lines, so
// screens that go with a switch (joining, losing VBand) leave it free.
//
// Sent whether or not the keyer has answered yet: several come from
// inside blocking calls (wifiConnect(), OTA) where the link can't be
// serviced, and the keyer is normally running well before we are.
void displayWifiConnecting();
void displayWifiPortal();
void displayWifiConnected(const IPAddress &ip);
void displayVbandJoined(const String &channel, bool firstJoin, const String &users);
void displayVbandLost();
void displayOtaStarting();
void displayOtaProgress(unsigned int progress, unsigned int total);
void displayOtaDone();
void displayOtaFailed();
void displayCircuitTest();

// Lines of their own in the keyer's scrolling conversation ("SYS"
// frames), rather than status screens.
void displayUserJoined(const String &tag);
void displayUserLeft(const String &tag);
