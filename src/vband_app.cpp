#include <Arduino.h>
#include "vband_app.h"
#include "wifi_setup.h"
#include "vband_client.h"
#include "ota_updater.h"
#include "keyer.h"
#include "channel_button.h"
#include "led_indicator.h"
#include "sidetone.h"
#include "mega_link.h"
#include "display_events.h"
#include "received_text.h"
#include "diagnostics.h"
#include "keyer_commands.h"
#include "config.h"
#include "adapter_settings.h"
#include "transcript.h"
#include "log.h"

namespace {

void onVbandRx(const String &userId, const String &userName, unsigned long space, unsigned long mark) {
  sidetoneQueueSpaceMark(space, mark, receivedTextSender(userId, userName));
}

// Our own keying goes to VBand, and into the transcript with it.
void onOwnSpaceMark(unsigned long space, unsigned long mark) {
  if (vbandIsJoined()) transcriptOwnSpaceMark(space, mark);
  vbandSendSpaceMark(space, mark);
}

// A press after a quiet spell shows where you are; pressing again within
// the switch window (VB.WIN) of the last press moves to the next channel
// (or, off VBand or in the lobby, connects and rejoins the last one).
void onChannelButton() {
  static bool pressedBefore = false;
  static unsigned long lastPressMs = 0;
  bool switching = pressedBefore && millis() - lastPressMs < adapterSetting(AdapterSetting::Window) * 1000UL;
  pressedBefore = true;
  lastPressMs = millis();
  if (switching) {
    vbandCycleChannel();
  } else {
    vbandShowRoom();
  }
}

}  // namespace

void vbandAppBegin() {
  keyerBegin();
  channelButtonBegin();
  ledBegin();
  sidetoneBegin();
  vbandSetRxCallback(onVbandRx);
  sidetoneSetPlayedCallback(receivedTextPlayed);
  megaLinkBegin(); // before wifiConnect(), which blocks
  keyerCommandsBegin();
  diagnosticsBegin();

  wifiConnect();
  transcriptBegin();
  vbandBegin();
  otaBegin();
}

void vbandAppLoop() {
  diagnosticsStageStart();
  vbandLoop();
  diagnosticsStageEnd(DIAG_VBAND);
  diagnosticsStageStart();
  megaLinkSetVbandReady(vbandIsReady());
  megaLinkLoop();
  keyerCommandsLoop();
  transcriptLoop(); // ahead of the log, which only takes what's left
  logForwardLoop();
  displayLoop();
  diagnosticsStageEnd(DIAG_MEGA);
  diagnosticsStageStart();
  keyerLoop(onOwnSpaceMark);
  diagnosticsStageEnd(DIAG_KEYER);
  diagnosticsStageStart();
  sidetoneLoop();
  diagnosticsStageEnd(DIAG_SIDETONE);
  diagnosticsStageStart();
  receivedTextLoop();
  diagnosticsStageEnd(DIAG_RXTEXT);
  diagnosticsStageStart();
  channelButtonLoop(onChannelButton);
  ledLoop();
  otaLoop();
  diagnosticsStageEnd(DIAG_OTHER);
  diagnosticsLoop();
}
