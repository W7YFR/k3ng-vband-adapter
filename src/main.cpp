#include <Arduino.h>
#include "wifi_setup.h"
#include "vband_client.h"
#include "ota_updater.h"
#include "keyer.h"
#include "channel_button.h"
#include "led_indicator.h"
#include "sidetone.h"
#include "power_latch.h"

void setup() {
  // Do not run any code before powerLatchBegin().
  // It is responsible for keeping the board on so
  // the button does not need to be held.
  powerLatchBegin();

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
