#include <Arduino.h>
#include "display_events.h"
#include "mega_link.h"
#include "config.h"
#include "adapter_settings.h"

namespace {

// How long each kind of screen stays up on the keyer before it goes back
// to what it was showing. Screens shown while we're stuck in a blocking
// call get the keyer's maximum, since nothing can replace them until it
// returns; the keyer caps any duration at 60s.
const unsigned long SHOW_BRIEF_MS = 3000;
const unsigned long SHOW_UNTIL_REPLACED_MS = 60000;
const unsigned long SHOW_ROOM_MS = 8000; // longer screens you asked for

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
  show(SHOW_BRIEF_MS, "WiFi OK|" + ip.toString() + "|Adapter " FIRMWARE_VERSION);
}

void displayLinkMismatch(int keyerProtocol, int ourProtocol) {
  show(SHOW_ROOM_MS, String("Link Mismatch|") +
                         (keyerProtocol < ourProtocol ? "Update keyer" : "Update adapter") +
                         "|Keyer v" + keyerProtocol + " Adptr v" + ourProtocol);
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

// Words into rows of up to KEYER_DISPLAY_COLUMNS, '|'-separated.
static String wrap(const String &words) {
  String rows;
  String row;
  int start = 0;
  while (start < (int)words.length()) {
    int end = words.indexOf(' ', start);
    if (end < 0) end = words.length();
    String word = words.substring(start, end);
    start = end + 1;
    if (row.length() && row.length() + 1 + word.length() > KEYER_DISPLAY_COLUMNS) {
      rows += (rows.length() ? "|" : "") + row;
      row = "";
    }
    row += (row.length() ? " " : "") + word;
  }
  if (row.length()) rows += (rows.length() ? "|" : "") + row;
  return rows;
}

void displayRoom(const String &channel, int count, const String &tags) {
  String rows = channel + "|";
  if (count < 0) {
    rows += "Listing...";
  } else if (count == 0) {
    rows += "Nobody else here";
  } else {
    rows += String(count) + " here|" + wrap(tags);
  }
  show(SHOW_ROOM_MS, rows);
}

void displayOtaWindowOpen(const IPAddress &ip, unsigned long minutes) {
  show(SHOW_ROOM_MS, "OTA Open|" + String(minutes) + " minutes|" + ip.toString());
}

void displayOtaAlwaysOn(const IPAddress &ip) {
  show(SHOW_ROOM_MS, "OTA On|" + ip.toString());
}

void displayVbandOff() {
  show(SHOW_ROOM_MS, "VBand Off|Press button twice|or /CON to connect");
}

void displayGoingToLobby() {
  show(SHOW_BRIEF_MS, "VBand|To the lobby...");
}

void displayNoOneOn() {
  show(SHOW_BRIEF_MS, "No One's On|All rooms empty");
}

// "?" for a count we don't have yet.
static String countText(int count) {
  return count < 0 ? String("?") : String(count);
}

void displayRoomCounts(const char *title, int lobby, int practice, const int *channels, int channelCount,
                       const String &custom, int customCount, bool briefly) {
  String rows = String(title) + "|Lobby " + countText(lobby) + " Prac " + countText(practice) + "|Ch";
  for (int i = 0; i < channelCount; i++) rows += " " + countText(channels[i]);
  rows += "|" + custom + " " + countText(customCount);
  show(briefly ? SHOW_BRIEF_MS : SHOW_ROOM_MS, rows);
}

void displayOtaWindowClosed() {
  show(SHOW_BRIEF_MS, "OTA Closed");
}

void displayPortalRestart() {
  show(SHOW_UNTIL_REPLACED_MS, "WiFi Setup|Restarting...");
}

void displayPowerOff() {
  show(SHOW_UNTIL_REPLACED_MS, "VBand|Powering Off");
}

void displayPowerOffFailed() {
  show(SHOW_BRIEF_MS, "Still Powered|(USB?) Staying On");
}

void displayDiagnostics(const String &rows) {
  show(SHOW_ROOM_MS, rows);
}

void displayCommandHelp() {
  show(SHOW_ROOM_MS, "WHO CH BUSY ROOMS|LOBBY CON DIS OTA|AP OFF DIAG");
}

void displayUnknownCommand(const String &word) {
  show(SHOW_BRIEF_MS, "Unknown Command|/" + word + "|/H for help");
}

void displayUserJoined(const String &tag) {
  if (adapterSetting(AdapterSetting::Join)) megaLinkSend("SYS", tag + " joined", true);
}

void displayUserLeft(const String &tag) {
  if (adapterSetting(AdapterSetting::Join)) megaLinkSend("SYS", tag + " left", true);
}
