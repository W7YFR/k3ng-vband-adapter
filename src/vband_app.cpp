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

void vbandAppBegin() {
  keyerBegin();
  channelButtonBegin();
  ledBegin();
  sidetoneBegin();
  vbandSetRxCallback(sidetoneQueueSpaceMark);
  megaLinkBegin(); // before wifiConnect(), which blocks

  wifiConnect();
  vbandBegin();
  otaBegin();
}

void vbandAppLoop() {
  vbandLoop();
  megaLinkLoop();
  keyerLoop(vbandSendSpaceMark);
  channelButtonLoop(vbandCycleChannel);
  ledLoop();
  sidetoneLoop();
  otaLoop();
}
