#include <Arduino.h>
#include "vband_app.h"
#include "wifi_setup.h"
#include "vband_client.h"
#include "ota_updater.h"
#include "keyer.h"
#include "channel_button.h"
#include "led_indicator.h"
#include "sidetone.h"

void vbandAppBegin() {
  keyerBegin();
  channelButtonBegin();
  ledBegin();
  sidetoneBegin();
  vbandSetRxCallback(sidetoneQueueSpaceMark);

  wifiConnect();
  vbandBegin();
  otaBegin();
}

void vbandAppLoop() {
  vbandLoop();
  keyerLoop(vbandSendSpaceMark);
  channelButtonLoop(vbandCycleChannel);
  ledLoop();
  sidetoneLoop();
  otaLoop();
}
