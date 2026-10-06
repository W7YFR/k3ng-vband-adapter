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
#include "received_text.h"
#include "diagnostics.h"
#include "keyer_commands.h"
#include "config.h"
#include "adapter_settings.h"

namespace {

void onVbandRx(const String &userId, const String &userName, unsigned long space, unsigned long mark) {
  sidetoneQueueSpaceMark(space, mark, receivedTextSender(userId, userName));
}

// A press after a quiet spell shows where you are; pressing again within
// the switch window (VB.WIN) of the last press moves to the next channel.
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
  diagnosticsStageEnd(DIAG_MEGA);
  diagnosticsStageStart();
  keyerLoop(vbandSendSpaceMark);
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
