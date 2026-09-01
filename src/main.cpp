#include <Arduino.h>
#include "wifi_setup.h"
#include "vband_client.h"
#include "ota_updater.h"
#include "keyer.h"
#include "channel_button.h"

void setup() {
  Serial.begin(115200);
  keyerBegin();
  channelButtonBegin();

  wifiConnect();
  vbandBegin();
  otaBegin();
}

void loop() {
  vbandLoop();
  keyerLoop(vbandSendSpaceMark);
  channelButtonLoop(vbandCycleChannel);
  otaLoop();
}
