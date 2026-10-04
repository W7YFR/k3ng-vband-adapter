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

  wifiConnect();
  vbandBegin();
  otaBegin();
}

void vbandAppLoop() {
  vbandLoop();
  megaLinkSetVbandReady(vbandIsReady());
  megaLinkLoop();
  keyerLoop(vbandSendSpaceMark);
  channelButtonLoop(vbandCycleChannel);
  ledLoop();
  sidetoneLoop();
  receivedTextLoop();
  otaLoop();
}
