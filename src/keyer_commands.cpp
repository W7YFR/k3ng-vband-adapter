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
#include "keyer_menu.h"
#include "config.h"
#include "log.h"

namespace {

// The commands as menu actions, in menu order: in the list the keyer asks
// for ("SL") and the menu (keyer_menu.cpp).
using Action = KeyerAction;
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

// "SQ,<key>[,<value>]": shows or sets one setting, for the keyer's keyed
// shortcuts and CLI; answered "SA,<1 or 0>,<what happened>", e.g.
// "SA,1,Keying: Off" or "SA,0,Bad value X".
void query(const String &fields) {
  int comma = fields.indexOf(',');
  String key = comma < 0 ? fields : fields.substring(0, comma);
  key.trim();
  int i = adapterSettingFind(key);
  if (i < 0) {
    megaLinkSend("SA", "0,Unknown vb." + key);
    return;
  }
  if (comma >= 0) {
    String token = fields.substring(comma + 1);
    token.trim();
    int value = 0;
    if (!token.length()) {
      megaLinkSend("SA", "0,Empty value");
      return;
    }
    if (!adapterSettingParse(i, token, &value)) {
      megaLinkSend("SA", "0,Bad value " + token);
      return;
    }
    if (adapterSettingSet(key, adapterSettingInfo(i).type == 's' ? token : String(value)) < 0) {
      megaLinkSend("SA", "0,Not changed");
      return;
    }
  }
  megaLinkSend("SA", String("1,") + adapterSettingInfo(i).label + ": " + adapterSettingShow(i, adapterSettingValue(i)));
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
// "SE,<count>"), for the keyer's CLI to print; "SQ" shows or sets one (see
// query()). The menu's frames go to keyer_menu.cpp.
void onKeyerFrame(const String &type, const String &fields) {
  if (keyerMenuFrame(type, fields)) return;
  if (type == "CK") {
    megaLinkSend("CR", isKnown(normalize(fields)) ? "1" : "0");
  } else if (type == "CMD") {
    runCommand(normalize(fields));
  } else if (type == "SL") {
    startList(fields);
  } else if (type == "SQ") {
    query(fields);
  }
}

}  // namespace

int keyerActionCount() {
  return ACTION_COUNT;
}

const KeyerAction &keyerAction(int i) {
  return ACTIONS[i];
}

bool keyerActionListed(int i) {
  return actionListed(ACTIONS[i]);
}

void keyerCommandRun(const String &word) {
  runCommand(normalize(word));
}

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
  keyerMenuLoop();

  if (listNext < 0 || millis() - lastListFrameMs < LIST_FRAME_SPACING_MS) return;
  lastListFrameMs = millis();
  if (listNext < listCount) {
    sendListItem(listNext++);
  } else {
    megaLinkSend("SE", String(listCount));
    listNext = -1;
  }
}
