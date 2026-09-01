#include <Arduino.h>
#include <WebSocketsClient.h>
#include "vband_client.h"
#include "config.h"

namespace {

WebSocketsClient ws;
bool joined = false;
String myId;

// Public channels named on the site, plus VBAND_CHANNEL (the custom room)
// last so a fresh boot's initial join lines up with vbandCycleChannel()'s
// next index. "Channel 5 (ND)" is the site's actual name, not a typo.
const char *CHANNEL_CYCLE[] = {
    "Channel 1", "Channel 2", "Channel 3", "Channel 4", "Channel 5 (ND)", VBAND_CHANNEL,
};
const int CHANNEL_CYCLE_COUNT = sizeof(CHANNEL_CYCLE) / sizeof(CHANNEL_CYCLE[0]);
int channelIndex = CHANNEL_CYCLE_COUNT - 1;

// Splits a comma-separated message into at most maxOut fields.
int splitFields(const String &msg, String *out, int maxOut) {
  int count = 0;
  int start = 0;
  while (count < maxOut) {
    int comma = msg.indexOf(',', start);
    if (comma == -1) {
      out[count++] = msg.substring(start);
      break;
    }
    out[count++] = msg.substring(start, comma);
    start = comma + 1;
  }
  return count;
}

void handleMessage(const String &msg) {
  String fields[6];
  int n = splitFields(msg, fields, 6);
  if (n < 1) return;
  const String &cmd = fields[0];

  if (cmd == "COK" && n >= 2) {
    myId = fields[1];
    Serial.println("Connected with id " + myId);
    ws.sendTXT("JC," + String(CHANNEL_CYCLE[channelIndex]));
  } else if (cmd == "CJN" && n >= 2) {
    joined = true;
    Serial.println("Joined channel " + fields[1]);
  } else if (cmd == "CNF" && n >= 2) {
    Serial.println("Server connection failure: " + fields[1]);
  } else if (cmd == "SMK" && n >= 6) {
    // SMK,<channel>,<user_id>,<user_name>,<space>,<mark> -- raw timing only,
    // the server never decodes to text. Letters would require porting
    // decoder.js's Morse decoder; for now just log the numbers.
    if (fields[2] != myId) {
      Serial.println("RX " + fields[3] + " space=" + fields[4] + " mark=" + fields[5]);
    }
  }
}

void onWsEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("WS connected");
      ws.sendTXT("CN," + String(VBAND_NAME) + ",0,VB 2.0");
      break;
    case WStype_DISCONNECTED:
      Serial.println("WS disconnected");
      joined = false;
      break;
    case WStype_TEXT: {
      String msg((char *)payload, length);
      msg.replace(String('\0'), ""); // server pads some frames with nulls
      handleMessage(msg);
      break;
    }
    default:
      break;
  }
}

}  // namespace

void vbandBegin() {
  ws.begin(VBAND_HOST, VBAND_PORT, VBAND_PATH, VBAND_PROTOCOL);
  ws.onEvent(onWsEvent);
  ws.setReconnectInterval(5000);
}

void vbandLoop() {
  ws.loop();
}

void vbandSendSpaceMark(unsigned long space, unsigned long mark) {
  if (!joined) return;
  Serial.println("TX space=" + String(space) + " mark=" + String(mark));
  ws.sendTXT("SM," + String(space) + "," + String(mark));
}

void vbandCycleChannel() {
  channelIndex = (channelIndex + 1) % CHANNEL_CYCLE_COUNT;
  joined = false; // gate SM sends until CJN confirms the new channel
  Serial.println("Switching to channel " + String(CHANNEL_CYCLE[channelIndex]));
  ws.sendTXT("JC," + String(CHANNEL_CYCLE[channelIndex]));
}
