#include <Arduino.h>
#include "wifi_setup.h"
#include "vband_client.h"
#include "ota_updater.h"
#include "keyer.h"
#include "channel_button.h"
#include "led_indicator.h"

void setup() {
  Serial.begin(115200);
  keyerBegin();
  channelButtonBegin();
  ledBegin();

  wifiConnect();
  vbandBegin();
  otaBegin();
}

void loop() {
  vbandLoop();
  keyerLoop(vbandSendSpaceMark);
  channelButtonLoop(vbandCycleChannel);
  ledLoop();
  otaLoop();
}
