#include <Arduino.h>
#include "wifi_setup.h"
#include "vband_client.h"
#include "ota_updater.h"
#include "keyer.h"
#include "channel_button.h"
#include "led_indicator.h"
#include "sidetone.h"

void setup() {
  Serial.begin(115200);
  keyerBegin();
  channelButtonBegin();
  ledBegin();
  sidetoneBegin();
  vbandSetRxCallback(sidetoneQueueSpaceMark);

  wifiConnect();
  vbandBegin();
  otaBegin();
}

void loop() {
  vbandLoop();
  keyerLoop(vbandSendSpaceMark);
  channelButtonLoop(vbandCycleChannel);
  ledLoop();
  sidetoneLoop();
  otaLoop();
}
