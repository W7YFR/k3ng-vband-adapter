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

namespace {

// The commands as menu actions, after the settings in the list the keyer
// asks for ("SL").
struct Action {
  const char *word;
  const char *label;
};
const Action ACTIONS[] = {
    {"WHO", "Who's here"}, {"CH", "Next channel"}, {"OTA", "OTA update"},
    {"AP", "WiFi setup"},  {"OFF", "Power off"},   {"DIAG", "Diagnostics"},
};
constexpr int ACTION_COUNT = sizeof(ACTIONS) / sizeof(ACTIONS[0]);

// The list goes out a frame at a time, spaced so the keyer's 64-byte
// receive buffer keeps up.
constexpr unsigned long LIST_FRAME_SPACING_MS = 20;
int listNext = -1; // next item to send, -1 when not listing
unsigned long lastListFrameMs = 0;

void sendListItem(int i) {
  int settings = adapterSettingsCount();
  if (i < settings) {
    megaLinkSend("SI", String(i) + "," + adapterSettingDescription(i));
  } else {
    const Action &a = ACTIONS[i - settings];
    megaLinkSend("SI", String(i) + "," + a.word + "," + a.label + ",a");
  }
}

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
  return word == "WHO" || word == "CH" || isChannelCommand(word) || word == "OTA" || word == "AP" ||
         word == "OFF" || word == "DIAG" || word == "H" || word == "HELP";
}

void runCommand(const String &word) {
  Serial.println("Keyer command: /" + word);

  if (word == "WHO") {
    vbandShowRoom();
  } else if (word == "CH") {
    vbandCycleChannel();
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
    listNext = 0;
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
  if (listNext < 0 || millis() - lastListFrameMs < LIST_FRAME_SPACING_MS) return;
  lastListFrameMs = millis();
  int count = adapterSettingsCount() + ACTION_COUNT;
  if (listNext < count) {
    sendListItem(listNext++);
  } else {
    megaLinkSend("SE", String(count));
    listNext = -1;
  }
}
