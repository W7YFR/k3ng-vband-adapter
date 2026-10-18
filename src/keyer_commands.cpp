#include <Arduino.h>
#include "keyer_commands.h"
#include "mega_link.h"
#include "vband_client.h"
#include "ota_updater.h"
#include "wifi_setup.h"
#include "power_latch.h"
#include "diagnostics.h"
#include "display_events.h"
#include "adapter_settings.h"
#include "vband_settings.h"
#include "user_tag.h"
#include "transcript.h"
#include "config.h"
#include "log.h"

namespace {

// The commands as menu actions, in the list the keyer asks for ("SL").
struct Action {
  const char *word;
  const char *label; // nullptr: not a command but the setting of that key, shown among them
  char group;        // the keyer's menu list it goes in (MENU_GROUP_*)
};
// In menu order; only the one of Connect / Disconnect that applies is listed.
const Action ACTIONS[] = {
    {"SEND", nullptr, MENU_GROUP_NONE},
    {"WHO", "Who's here", MENU_GROUP_ROOMS},      {"CHAN", nullptr, MENU_GROUP_ROOMS},
    {"CH", "Next channel", MENU_GROUP_ROOMS},     {"BUSY", "Busiest room", MENU_GROUP_ROOMS},
    {"ROOMS", "Room counts", MENU_GROUP_ROOMS},   {"LOBBY", "To lobby", MENU_GROUP_ROOMS},
    {"CON", "Connect", MENU_GROUP_DEVICE},        {"DIS", "Disconnect", MENU_GROUP_DEVICE},
    {"OTA", "OTA 10 min", MENU_GROUP_DEVICE},     {"AP", "WiFi setup", MENU_GROUP_DEVICE},
    {"OFF", "Power off", MENU_GROUP_DEVICE},      {"DIAG", "Diagnostics", MENU_GROUP_DEVICE},
    {"LAST", "Last drops", MENU_GROUP_DEVICE},
};
constexpr int ACTION_COUNT = sizeof(ACTIONS) / sizeof(ACTIONS[0]);

// Only the one of Connect / Disconnect that applies goes in the list.
bool actionListed(const Action &a) {
  if (!strcmp(a.word, "CON")) return !vbandIsOn();
  if (!strcmp(a.word, "DIS")) return vbandIsOn();
  return true;
}

// The list goes out a frame at a time, spaced so the keyer's 64-byte
// receive buffer keeps up. "SL,s" lists just the settings and "SL,a" just
// the commands (the keyer only holds one half at a time); "SL" both. Items
// are numbered from 0 in what's sent.
constexpr unsigned long LIST_FRAME_SPACING_MS = 20;
int list[48];       // what's being listed: settings 0.., then commands (settings count + action index)
int listCount = 0;
int listNext = -1;  // next entry to send, -1 when not listing
unsigned long lastListFrameMs = 0;

// Settings listed among the commands (a lower-case group) go with them,
// where ACTIONS places them, not with the settings.
bool amongCommands(int setting) {
  return islower(adapterSettingDescription(setting)[0]);
}

void startList(const String &kind) {
  int settings = adapterSettingsCount();
  listCount = 0;
  if (kind != "a") {
    for (int i = 0; i < settings; i++) {
      if (!amongCommands(i)) list[listCount++] = i;
    }
  }
  if (kind != "s") {
    for (int i = 0; i < ACTION_COUNT; i++) {
      if (actionListed(ACTIONS[i])) list[listCount++] = settings + i;
    }
  }
  listNext = 0;
}

void sendListItem(int n) {
  int i = list[n];
  int settings = adapterSettingsCount();
  String index = String(n) + ",";
  if (i < settings) {
    megaLinkSend("SI", index + adapterSettingDescription(i));
  } else if (!ACTIONS[i - settings].label) {
    megaLinkSend("SI", index + adapterSettingDescription(adapterSettingFind(ACTIONS[i - settings].word)));
  } else {
    const Action &a = ACTIONS[i - settings];
    megaLinkSend("SI", index + a.group + "," + a.word + "," + a.label + ",a");
  }
}

// Our VBand name's tag, for the keyer to show our own sending under.
String lastTagSent;
bool linkWasUp = false;

void sendValue(const String &key, int i) {
  megaLinkSend("SV", key + "," + (i < 0 ? String("?") : adapterSettingValueText(i)));
}

String normalize(String word) {
  word.trim();
  word.toUpperCase();
  return word;
}

bool isChannelCommand(const String &word) {
  return word.length() == 3 && word.startsWith("CH") && vbandChannelCodeValid(word[2]);
}

// The keyer asks before leaving command mode, so a typo can be retried.
bool isKnown(const String &word) {
  for (const Action &a : ACTIONS) {
    if (a.label && word == a.word) return true;
  }
  return isChannelCommand(word) || word == "H" || word == "HELP";
}

void runCommand(const String &word) {
  Log.println("Keyer command: /" + word);

  if (word == "WHO") {
    vbandShowRoom();
    // Off VBand: offer to connect ("OF,<command>,<label>"); the keyer's menu
    // runs it if you key R while the answer shows.
    if (!vbandIsOn()) megaLinkSend("OF", "CON,Connect");
  } else if (word == "CH") {
    vbandCycleChannel();
  } else if (word == "BUSY") {
    vbandJoinBusiest();
  } else if (word == "ROOMS") {
    vbandShowCounts();
  } else if (word == "LOBBY") {
    vbandGoToLobby();
  } else if (word == "CON") {
    vbandConnect();
  } else if (word == "DIS") {
    vbandDisconnect();
  } else if (isChannelCommand(word)) {
    vbandJoinChannelCode(word[2]); // the join screen answers
  } else if (word == "OTA") {
    otaOpenWindow();
  } else if (word == "AP") {
    wifiRestartIntoPortal();
  } else if (word == "OFF") {
    displayPowerOff();
    powerOffNow();
  } else if (word == "DIAG") {
    displayDiagnostics(diagnosticsSummary());
    diagnosticsLogEvents();
    Log.printf("DIAG transcript: %u lines, %u bytes recorded\n", (unsigned)transcriptRecordedLines(),
               (unsigned)transcriptRecordedBytes());
  } else if (word == "LAST") {
    displayDiagnostics(diagnosticsLastEvents());
    diagnosticsLogEvents();
  } else if (word == "H" || word == "HELP") {
    displayCommandHelp();
  } else {
    displayUnknownCommand(word);
  }
}

// "CK,<word>": is this a command? answered "CR,1" / "CR,0". Then, once the
// keyer has left command mode (where it wouldn't show our answer),
// "CMD,<word>" runs it.
// Settings: "SL" lists them and the commands ("SI,<i>,..." each, then
// "SE,<count>"); "SG,<key>" reads one and "SS,<key>,<value>" sets it, both
// answered "SV,<key>,<value>" ("?" for no such setting).
void onKeyerFrame(const String &type, const String &fields) {
  if (type == "CK") {
    megaLinkSend("CR", isKnown(normalize(fields)) ? "1" : "0");
  } else if (type == "CMD") {
    runCommand(normalize(fields));
  } else if (type == "SL") {
    startList(fields);
  } else if (type == "SG") {
    String key = normalize(fields);
    sendValue(key, adapterSettingFind(key));
  } else if (type == "SS") {
    int comma = fields.indexOf(',');
    String key = normalize(comma < 0 ? fields : fields.substring(0, comma));
    int i = comma < 0 ? -1 : adapterSettingSet(key, fields.substring(comma + 1));
    sendValue(key, i);
  }
}

}  // namespace

void keyerCommandsBegin() {
  megaLinkSetFrameCallback(onKeyerFrame);
}

void keyerCommandsLoop() {
  // "MY,<tag>": the tag the keyer shows our own sending under, from our
  // VBand name the way everyone else's is found. Again whenever the link
  // comes back (the keyer may have restarted) or the name changes.
  bool linkUp = megaLinkUp();
  String tag = userTag(vbandSettingsName());
  if (linkUp && (!linkWasUp || tag != lastTagSent) && megaLinkSend("MY", tag)) lastTagSent = tag;
  // "SN,1" / "SN,0": whether our keying goes to VBand (VB.SEND), so the
  // keyer only tags our sending with our name while it does. On change and
  // every couple of seconds.
  static int sendingSent = -1;
  static unsigned long sendingSentMs = 0;
  int sending = vbandSending() ? 1 : 0;
  if (linkUp && (sending != sendingSent || !linkWasUp || millis() - sendingSentMs >= MEGA_LINK_HI_UP_MS) &&
      megaLinkSend("SN", String(sending))) {
    sendingSent = sending;
    sendingSentMs = millis();
  }
  linkWasUp = linkUp;

  if (listNext < 0 || millis() - lastListFrameMs < LIST_FRAME_SPACING_MS) return;
  lastListFrameMs = millis();
  if (listNext < listCount) {
    sendListItem(listNext++);
  } else {
    megaLinkSend("SE", String(listCount));
    listNext = -1;
  }
}
