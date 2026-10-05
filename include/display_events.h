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
void displayVbandConnecting();
void displayVbandLost();
// A connection attempt failed before VBand was ever ready (on boot or
// after losing it); says whether WiFi itself is down.
void displayVbandUnreachable(bool wifiUp);
void displayOtaStarting();
void displayOtaProgress(unsigned int progress, unsigned int total);
void displayOtaDone();
void displayOtaFailed();
void displayCircuitTest();

// The keyer speaks a different link protocol version than we do.
void displayLinkMismatch(int keyerProtocol, int ourProtocol);

// Current channel and who's in it; count -1 while the list isn't in yet.
void displayRoom(const String &channel, int count, const String &tags);

// Answers to commands keyed in the keyer's command mode ("/OTA" etc.).
void displayOtaWindowOpen(const IPAddress &ip, unsigned long minutes);
void displayOtaAlwaysOn(const IPAddress &ip);
void displayOtaWindowClosed();
void displayPortalRestart();
void displayPowerOff();
void displayPowerOffFailed();
void displayDiagnostics(const String &rows);
void displayCommandHelp();
void displayUnknownCommand(const String &word);

// Lines of their own in the keyer's scrolling conversation ("SYS"
// frames), rather than status screens.
void displayUserJoined(const String &tag);
void displayUserLeft(const String &tag);
