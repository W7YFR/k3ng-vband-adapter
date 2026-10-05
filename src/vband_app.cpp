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

namespace {

void onVbandRx(const String &userId, const String &userName, unsigned long space, unsigned long mark) {
  sidetoneQueueSpaceMark(space, mark, receivedTextSender(userId, userName));
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
  channelButtonLoop(vbandCycleChannel);
  ledLoop();
  otaLoop();
  diagnosticsStageEnd(DIAG_OTHER);
  diagnosticsLoop();
}
