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

// rows: up to four rows separated by '|'. Each is cut to the keyer's
// width (it would cut them there anyway) and the whole screen to what fits
// in one frame, so long room or user names can't get it dropped.
void show(unsigned long ms, const String &rows) {
  String fields = String(ms) + ",";
  int start = 0;
  while (start <= (int)rows.length()) {
    int end = rows.indexOf('|', start);
    if (end < 0) end = rows.length();
    if (start > 0) fields += '|';
    fields += rows.substring(start, min(end, start + KEYER_DISPLAY_COLUMNS));
    start = end + 1;
  }
  const int maxFields = MEGA_LINK_MAX_FRAME - 8; // "$ST," and "*XX\n"
  if ((int)fields.length() > maxFields) fields = fields.substring(0, maxFields);
  megaLinkSend("ST", fields, true);
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

void displayVbandJoined(const String &channel, bool firstJoin, const String &users) {
  show(SHOW_BRIEF_MS, (firstJoin ? "VBand On|" : "Channel|") + channel + "|" + users);
}

void displayVbandConnecting() {
  show(SHOW_UNTIL_REPLACED_MS, "VBand|Connecting...");
}

void displayVbandUnreachable(bool wifiUp) {
  show(SHOW_UNTIL_REPLACED_MS, wifiUp ? "VBand Offline|Retrying..." : "WiFi Lost|Retrying...");
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

void displayUserJoined(const String &tag) {
  megaLinkSend("SYS", tag + " joined", true);
}

void displayUserLeft(const String &tag) {
  megaLinkSend("SYS", tag + " left", true);
}
