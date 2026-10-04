#include <Arduino.h>
#include "display_events.h"
#include "mega_link.h"
#include "config.h"

namespace {

// How long each kind of screen stays up on the keyer before it goes back
// to what it was showing. Screens shown while we're stuck in a blocking
// call get the keyer's maximum, since nothing can replace them until it
// returns; the keyer caps any duration at 60s.
const unsigned long SHOW_BRIEF_MS = 3000;
const unsigned long SHOW_UNTIL_REPLACED_MS = 60000;

// rows: up to four rows separated by '|'.
void show(unsigned long ms, const String &rows) {
  megaLinkSend("ST", String(ms) + "," + rows, true);
}

}  // namespace

void displayWifiConnecting() {
  show(SHOW_UNTIL_REPLACED_MS, "WiFi...");
}

void displayWifiPortal() {
  show(SHOW_UNTIL_REPLACED_MS, "WiFi Setup|Join AP|" WIFI_MANAGER_AP_NAME);
}

void displayWifiConnected(const IPAddress &ip) {
  show(SHOW_BRIEF_MS, "WiFi OK|" + ip.toString());
}

void displayVbandJoined(const String &channel, bool firstJoin) {
  show(SHOW_BRIEF_MS, (firstJoin ? "VBand On|" : "Channel|") + channel);
}

void displayVbandLost() {
  show(SHOW_BRIEF_MS, "VBand Lost|Reconnecting");
}

void displayOtaStarting() {
  show(SHOW_UNTIL_REPLACED_MS, "OTA Update|Starting");
}

void displayOtaProgress(unsigned int progress, unsigned int total) {
  static int lastTenth = -1;
  if (total == 0) return;
  int tenth = (int)((uint64_t)progress * 10 / total);
  if (progress == 0) lastTenth = -1; // a new upload
  if (tenth == lastTenth) return;
  lastTenth = tenth;
  show(SHOW_UNTIL_REPLACED_MS, "OTA Update|" + String(tenth * 10) + "%");
}

void displayOtaDone() {
  show(SHOW_BRIEF_MS, "OTA Done|Rebooting");
}

void displayOtaFailed() {
  show(SHOW_BRIEF_MS, "OTA Failed");
}

void displayCircuitTest() {
  show(SHOW_BRIEF_MS, "VBand Test Mode|Press button|for test screens");
}
