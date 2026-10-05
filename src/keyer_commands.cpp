#include <Arduino.h>
#include "keyer_commands.h"
#include "mega_link.h"
#include "vband_client.h"
#include "ota_updater.h"
#include "wifi_setup.h"
#include "power_latch.h"
#include "diagnostics.h"
#include "display_events.h"

namespace {

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
void onKeyerFrame(const String &type, const String &fields) {
  if (type == "CK") {
    megaLinkSend("CR", isKnown(normalize(fields)) ? "1" : "0");
  } else if (type == "CMD") {
    runCommand(normalize(fields));
  }
}

}  // namespace

void keyerCommandsBegin() {
  megaLinkSetFrameCallback(onKeyerFrame);
}
