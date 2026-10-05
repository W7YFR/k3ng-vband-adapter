#include <Arduino.h>
#include <WebSocketsClient.h>
#include "vband_client.h"
#include "vband_settings.h"
#include "config.h"
#include "display_events.h"
#include "room_users.h"
#include "wifi_setup.h"

namespace {

WebSocketsClient ws;
bool joined = false;
bool ready = false;
String myId;
String joinedChannel;

// After a join, the join screen (and "ready", whose key line switch on
// the keyer adds "TX 2" under that screen) waits for the channel's user
// list, up to JOIN_SCREEN_WAIT_MS.
bool joinScreenPending = false;
bool joinScreenFirst = false;
unsigned long joinedAtMs = 0;
unsigned long lastUserListRequestMs = 0;
VbandRxSpaceMarkCallback rxCallback = nullptr;

// Public channels named on the site, followed by the custom room
// (vbandSettingsRoom()) as the last cycle position, so a first boot (no
// channel saved yet) starts on the custom room and the button then goes
// to Channel 1. After that, boot rejoins the last channel joined.
// "Channel 5 (ND)" is the site's actual name, not a typo.
const char *PUBLIC_CHANNELS[] = {
    "Channel 1", "Channel 2", "Channel 3", "Channel 4", "Channel 5 (ND)",
};
const int PUBLIC_CHANNEL_COUNT = sizeof(PUBLIC_CHANNELS) / sizeof(PUBLIC_CHANNELS[0]);
const int CHANNEL_CYCLE_COUNT = PUBLIC_CHANNEL_COUNT + 1;
int channelIndex = CHANNEL_CYCLE_COUNT - 1;

String currentChannel() {
  if (channelIndex < PUBLIC_CHANNEL_COUNT) return PUBLIC_CHANNELS[channelIndex];
  return vbandSettingsRoom();
}

void requestUserList() {
  ws.sendTXT("LU," + joinedChannel);
  lastUserListRequestMs = millis();
}

void finishJoin() {
  joinScreenPending = false;
  displayVbandJoined(joinedChannel, joinScreenFirst,
                     roomUsersKnown() ? roomUsersSummary() : String());
  ready = true;
}

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
    ws.sendTXT("JC," + currentChannel());
  } else if (cmd == "CJN" && n >= 2) {
    joined = true;
    joinedChannel = fields[1];
    if (joinedChannel == currentChannel()) vbandSettingsSaveChannel(channelIndex);
    joinScreenPending = true;
    joinScreenFirst = !ready;
    joinedAtMs = millis();
    roomUsersReset(joinedChannel, myId);
    requestUserList();
    Serial.println("Joined channel " + fields[1]);
  } else if (cmd == "ULB" && n >= 2) {
    roomUsersListBegin(fields[1]);
  } else if (cmd == "ULE" && n >= 4) {
    roomUsersListAdd(fields[1], fields[2], fields[3]);
  } else if (cmd == "ULC" && n >= 2) {
    roomUsersListComplete(fields[1]);
    if (joinScreenPending && roomUsersKnown()) finishJoin();
  } else if (cmd == "CUP" && n >= 3) {
    if (joined && fields[1] == joinedChannel) requestUserList();
  } else if (cmd == "CNF" && n >= 2) {
    Serial.println("Server connection failure: " + fields[1]);
  } else if (cmd == "SMK" && n >= 6) {
    // SMK,<channel>,<user_id>,<user_name>,<space>,<mark> -- raw timing only,
    // the server never decodes to text (see received_text.cpp for that).
    if (fields[2] != myId) {
      Serial.println("RX " + fields[3] + " space=" + fields[4] + " mark=" + fields[5]);
      if (rxCallback) {
        rxCallback(fields[2], fields[3], fields[4].toInt(), fields[5].toInt());
      }
    }
  } else if (cmd != "CLB" && cmd != "CLE" && cmd != "CLC" && cmd != "PON") {
    Serial.println("Unhandled: " + msg); // learning what else the server sends
  }
}

void onWsEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("WS connected");
      ws.sendTXT("CN," + vbandSettingsName() + ",0,VB 2.0");
      break;
    case WStype_DISCONNECTED:
      Serial.println("WS disconnected");
      // Fires again on every failed reconnect attempt, roughly every 5s.
      if (ready) {
        displayVbandLost();
      } else {
        displayVbandUnreachable(wifiConnected());
      }
      joined = false;
      ready = false;
      joinScreenPending = false;
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
  int saved = vbandSettingsChannel();
  if (saved >= 0 && saved < CHANNEL_CYCLE_COUNT) channelIndex = saved;
  displayVbandConnecting();
  ws.begin(VBAND_HOST, VBAND_PORT, VBAND_PATH, VBAND_PROTOCOL);
  ws.onEvent(onWsEvent);
  ws.setReconnectInterval(5000);
}

void vbandLoop() {
  ws.loop();

  if (joinScreenPending && millis() - joinedAtMs >= JOIN_SCREEN_WAIT_MS) {
    finishJoin(); // the user list didn't come; don't hold up VBand for it
  }
  if (joined && millis() - lastUserListRequestMs >= ROOM_USERS_POLL_MS) {
    requestUserList();
  }
}

void vbandSendSpaceMark(unsigned long space, unsigned long mark) {
  if (!joined) return;
  Serial.println("TX space=" + String(space) + " mark=" + String(mark));
  ws.sendTXT("SM," + String(space) + "," + String(mark));
}

void vbandCycleChannel() {
  channelIndex = (channelIndex + 1) % CHANNEL_CYCLE_COUNT;
  joined = false; // gate SM sends until CJN confirms the new channel
  Serial.println("Switching to channel " + currentChannel());
  ws.sendTXT("JC," + currentChannel());
}

bool vbandIsJoined() {
  return joined;
}

bool vbandIsReady() {
  return ready;
}

String vbandChannelName() {
  return currentChannel();
}

char vbandChannelCode() {
  if (channelIndex == CHANNEL_CYCLE_COUNT - 1) return 'C'; // custom room
  return '1' + channelIndex;
}

void vbandSetRxCallback(VbandRxSpaceMarkCallback callback) {
  rxCallback = callback;
}
