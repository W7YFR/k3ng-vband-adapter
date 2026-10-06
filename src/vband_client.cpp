#include <Arduino.h>
#include <WebSocketsClient.h>
#include "vband_client.h"
#include "vband_settings.h"
#include "adapter_settings.h"
#include "config.h"
#include "display_events.h"
#include "room_users.h"
#include "wifi_setup.h"
#include "diagnostics.h"

namespace {

WebSocketsClient ws;
bool wsStarted = false;
bool connectionWanted = false; // false: disconnected on purpose (VB.START off, /DIS)
bool connected = false;        // the server has accepted us (COK)
bool wantRoom = false;         // join channelIndex once connected; false: stay in the lobby
bool quietDisconnect = false;  // dropping the connection on purpose to land in the lobby
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
// to Channel 1. After that, boot rejoins the last channel joined (or
// whatever VB.START says).
// "Channel 5 (ND)" is the site's actual name, not a typo.
const char *PUBLIC_CHANNELS[] = {
    "Channel 1", "Channel 2", "Channel 3", "Channel 4", "Channel 5 (ND)",
};
const int PUBLIC_CHANNEL_COUNT = sizeof(PUBLIC_CHANNELS) / sizeof(PUBLIC_CHANNELS[0]);
const int CHANNEL_CYCLE_COUNT = PUBLIC_CHANNEL_COUNT + 1;
const int CUSTOM_INDEX = CHANNEL_CYCLE_COUNT - 1;
int channelIndex = CUSTOM_INDEX;

// How many are in each room: the server lists the public ones ("LC" ->
// CLB, CLE,<name>,<count>..., CLC) and updates them (CUP); our custom
// room is private, so it's counted from its user list ("LU").
const char *LOBBY = "Lobby";
const char *PRACTICE = "Practice Channel";
int lobbyCount = -1;
int practiceCount = -1;
int channelCounts[PUBLIC_CHANNEL_COUNT] = {-1, -1, -1, -1, -1};
int customCount = -1;
int customListing = -1; // users in the custom room's list being received
// What to do once the counts are in (CLC).
enum class CountsFor { Nothing, Splash, Show, Busiest };
CountsFor countsFor = CountsFor::Nothing;
unsigned long countsAskedMs = 0;
constexpr unsigned long COUNTS_WAIT_MS = 2000;
// After the connect splash, hold the join long enough to read it.
unsigned long joinAtMs = 0;
constexpr unsigned long SPLASH_MS = 3000;

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

void sendJoin() {
  joined = false; // gate SM sends until CJN confirms the new channel
  Serial.println("Switching to channel " + currentChannel());
  ws.sendTXT("JC," + currentChannel());
}

void askCounts(CountsFor purpose) {
  countsFor = purpose;
  countsAskedMs = millis();
  ws.sendTXT("LU," + vbandSettingsRoom()); // before LC, so it's usually in by CLC
  ws.sendTXT("LC");
}

void setCount(const String &channel, int count) {
  if (channel == LOBBY) lobbyCount = count;
  if (channel == PRACTICE) practiceCount = count;
  for (int i = 0; i < PUBLIC_CHANNEL_COUNT; i++) {
    if (channel == PUBLIC_CHANNELS[i]) channelCounts[i] = count;
  }
  if (channel == vbandSettingsRoom()) customCount = count;
}

// Count in a channel-cycle position, not counting ourselves.
int othersAt(int index) {
  int count = index < PUBLIC_CHANNEL_COUNT ? channelCounts[index] : customCount;
  if (count > 0 && joined && index == channelIndex) count--;
  return count;
}

void showCounts(bool briefly) {
  displayRoomCounts(connected && !wantRoom ? "Lobby" : "Rooms", lobbyCount, practiceCount, channelCounts,
                    PUBLIC_CHANNEL_COUNT, vbandSettingsRoom(), customCount, briefly);
}

void joinBusiest() {
  int best = -1;
  for (int i = 0; i < CHANNEL_CYCLE_COUNT; i++) {
    if (othersAt(i) > 0 && (best < 0 || othersAt(i) > othersAt(best))) best = i;
  }
  if (best < 0) {
    displayNoOneOn();
    return;
  }
  wantRoom = true;
  channelIndex = best;
  sendJoin();
}

void countsComplete() {
  CountsFor purpose = countsFor;
  countsFor = CountsFor::Nothing;
  switch (purpose) {
    case CountsFor::Splash:
      showCounts(true);
      if (wantRoom) joinAtMs = millis() + SPLASH_MS;
      break;
    case CountsFor::Show:
      showCounts(false);
      break;
    case CountsFor::Busiest:
      joinBusiest();
      break;
    case CountsFor::Nothing:
      break;
  }
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
    connected = true;
    Serial.println("Connected with id " + myId);
    askCounts(CountsFor::Splash); // the join (if any) follows the splash
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
    if (fields[1] == vbandSettingsRoom()) customListing = 0;
    roomUsersListBegin(fields[1]);
  } else if (cmd == "ULE" && n >= 4) {
    if (fields[1] == vbandSettingsRoom() && customListing >= 0) customListing++;
    roomUsersListAdd(fields[1], fields[2], fields[3]);
  } else if (cmd == "ULC" && n >= 2) {
    if (fields[1] == vbandSettingsRoom() && customListing >= 0) {
      customCount = customListing;
      customListing = -1;
    }
    roomUsersListComplete(fields[1]);
    if (joinScreenPending && roomUsersKnown()) finishJoin();
  } else if (cmd == "CLE" && n >= 3) {
    setCount(fields[1], fields[2].toInt());
  } else if (cmd == "CLC") {
    countsComplete();
  } else if (cmd == "CUP" && n >= 3) {
    setCount(fields[1], fields[2].toInt());
    if (joined && fields[1] == joinedChannel) requestUserList();
  } else if (cmd == "CNF" && n >= 2) {
    Serial.println("Server connection failure: " + fields[1]);
  } else if (cmd == "SMK" && n >= 6) {
    // SMK,<channel>,<user_id>,<user_name>,<space>,<mark> -- raw timing only,
    // the server never decodes to text (see received_text.cpp for that).
    if (fields[2] != myId) {
      diagnosticsNoteSmk();
      Serial.println("RX " + fields[3] + " space=" + fields[4] + " mark=" + fields[5]);
      if (rxCallback) {
        rxCallback(fields[2], fields[3], fields[4].toInt(), fields[5].toInt());
      }
    }
  } else if (cmd != "CLB" && cmd != "PON") {
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
      diagnosticsNoteWsDisconnect();
      // Fires again on every failed reconnect attempt, roughly every 5s.
      if (quietDisconnect) {
        quietDisconnect = false;
        displayGoingToLobby();
      } else if (connectionWanted) {
        if (ready) {
          displayVbandLost();
        } else {
          displayVbandUnreachable(wifiConnected());
        }
      }
      connected = false;
      joined = false;
      ready = false;
      joinScreenPending = false;
      joinAtMs = 0;
      countsFor = CountsFor::Nothing;
      break;
    case WStype_TEXT: {
      String msg((char *)payload, length);
      msg.replace(String('\0'), ""); // server pads some frames with nulls
      diagnosticsNoteServerMessage();
      handleMessage(msg);
      break;
    }
    default:
      break;
  }
}

void startConnecting() {
  connectionWanted = true;
  displayVbandConnecting();
  if (!wsStarted) {
    wsStarted = true;
    ws.begin(VBAND_HOST, VBAND_PORT, VBAND_PATH, VBAND_PROTOCOL);
    ws.onEvent(onWsEvent);
    ws.setReconnectInterval(5000);
  }
}

// Called with a channel wanted (a channel button press, /CH...): connect
// first if we aren't, else join now.
void goToChannel(int index) {
  channelIndex = index;
  wantRoom = true;
  if (!connectionWanted) {
    startConnecting();
  } else if (connected) {
    joinAtMs = 0;
    sendJoin();
  }
}

}  // namespace

void vbandBegin() {
  int saved = vbandSettingsChannel();
  if (saved >= 0 && saved < CHANNEL_CYCLE_COUNT) channelIndex = saved;
  switch (adapterSetting(AdapterSetting::Start)) {
    case VBAND_START_OFF:
      displayVbandOff();
      return;
    case VBAND_START_LOBBY:
      wantRoom = false;
      break;
    case VBAND_START_CUSTOM:
      channelIndex = CUSTOM_INDEX;
      wantRoom = true;
      break;
    default: // VBAND_START_LAST
      wantRoom = true;
      break;
  }
  startConnecting();
}

void vbandLoop() {
  if (!connectionWanted) return; // disconnected on purpose: leave the socket alone
  ws.loop();

  if (countsFor != CountsFor::Nothing && millis() - countsAskedMs >= COUNTS_WAIT_MS) {
    countsComplete(); // the list didn't come; carry on with what we know
  }
  if (joinAtMs && (long)(millis() - joinAtMs) >= 0) {
    joinAtMs = 0;
    if (connected && wantRoom) sendJoin();
  }
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
  // From the lobby (or disconnected), go back to the channel we were on.
  bool inRoom = connectionWanted && connected && wantRoom;
  goToChannel(inRoom ? (channelIndex + 1) % CHANNEL_CYCLE_COUNT : channelIndex);
}

bool vbandChannelCodeValid(char code) {
  code = toupper(code);
  return code == 'C' || (code >= '1' && code < '1' + PUBLIC_CHANNEL_COUNT);
}

bool vbandJoinChannelCode(char code) {
  if (!vbandChannelCodeValid(code)) return false;
  code = toupper(code);
  goToChannel(code == 'C' ? CUSTOM_INDEX : code - '1');
  return true;
}

void vbandConnect() {
  if (connectionWanted) {
    if (connected) showCounts(false);
    return;
  }
  wantRoom = false; // a plain connect waits in the lobby
  startConnecting();
}

void vbandDisconnect() {
  if (!connectionWanted) return;
  Serial.println("Disconnecting from VBand");
  connectionWanted = false;
  ws.disconnect();
  connected = false;
  joined = false;
  ready = false;
  joinScreenPending = false;
  joinAtMs = 0;
  countsFor = CountsFor::Nothing;
  displayVbandOff();
}

void vbandGoToLobby() {
  if (!connectionWanted) {
    vbandConnect();
    return;
  }
  if (!joined && !joinAtMs) {
    showCounts(false); // already there
    return;
  }
  // The server has no "leave channel"; a fresh connection starts in the lobby.
  Serial.println("Going to the lobby");
  wantRoom = false;
  joinAtMs = 0;
  quietDisconnect = true;
  ws.disconnect(); // the library reconnects by itself
}

void vbandShowCounts() {
  if (!connected) {
    if (connectionWanted) {
      displayVbandUnreachable(wifiConnected());
    } else {
      displayVbandOff();
    }
    return;
  }
  askCounts(CountsFor::Show);
}

void vbandJoinBusiest() {
  if (!connected) {
    vbandShowCounts();
    return;
  }
  askCounts(CountsFor::Busiest);
}

void vbandNameChanged() {
  if (connected) ws.sendTXT("NC," + vbandSettingsName()); // renames us without reconnecting
}

void vbandCustomRoomChanged() {
  customCount = -1;
  if (joined && channelIndex == CUSTOM_INDEX) sendJoin();
}

void vbandShowRoom() {
  if (!connectionWanted) {
    displayVbandOff();
    return;
  }
  if (connected && !wantRoom) {
    showCounts(false); // in the lobby
    return;
  }
  if (!joined) {
    displayVbandUnreachable(wifiConnected());
    return;
  }
  displayRoom(joinedChannel, roomUsersKnown() ? roomUsersCount() : -1, roomUsersTags());
}

bool vbandIsJoined() {
  return joined;
}

bool vbandIsReady() {
  return ready;
}

bool vbandIsConnected() {
  return connected;
}

bool vbandIsOn() {
  return connectionWanted;
}

bool vbandIsJoining() {
  return connectionWanted && (!connected || (wantRoom && !joined));
}

String vbandChannelName() {
  return currentChannel();
}

char vbandChannelCode() {
  if (channelIndex == CUSTOM_INDEX) return 'C'; // custom room
  return '1' + channelIndex;
}

void vbandSetRxCallback(VbandRxSpaceMarkCallback callback) {
  rxCallback = callback;
}
