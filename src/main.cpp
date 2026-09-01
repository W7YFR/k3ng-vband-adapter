#include <Arduino.h>
#include <WiFiManager.h>
#include <WebSocketsClient.h>
#include <ArduinoOTA.h>
#include "pins.h"
#include "config.h"

WebSocketsClient ws;
bool joined = false;
String myId;

// ---- keying: non-blocking debounce of the keying line. ----
bool keyLastRaw = false;
bool keyState = false;
unsigned long keyLastChangeMs = 0;
unsigned long downStartMs = 0;
unsigned long lastReleaseMs = 0;

// ---- channel button: non-blocking debounce, fires once per press. ----
bool buttonLastRaw = false;
bool buttonState = false;
unsigned long buttonLastChangeMs = 0;

// Public channels named on the site, plus VBAND_CHANNEL (the custom room)
// last so a fresh boot's initial join lines up with cycleChannel()'s next
// index. "Channel 5 (ND)" is the site's actual name, not a typo.
const char *CHANNEL_CYCLE[] = {
  "Channel 1",
  "Channel 2",
  "Channel 3",
  "Channel 4",
  "Channel 5 (ND)",
  VBAND_CHANNEL,
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

void updateKey() {
  bool raw = digitalRead(PIN_KEY) == LOW;
  unsigned long now = millis();

  if (raw != keyLastRaw) {
    keyLastRaw = raw;
    keyLastChangeMs = now;
    return;
  }

  if (raw != keyState && now - keyLastChangeMs >= DEBOUNCE_MS) {
    keyState = raw;
    if (keyState) {
      downStartMs = now;
    } else {
      unsigned long space = min(downStartMs - lastReleaseMs, (unsigned long)MAX_TIME_MS);
      unsigned long mark = min(now - downStartMs, (unsigned long)MAX_TIME_MS);
      lastReleaseMs = now;
      if (joined) {
        Serial.println("TX space=" + String(space) + " mark=" + String(mark));
        ws.sendTXT("SM," + String(space) + "," + String(mark));
      }
    }
  }
}

void cycleChannel() {
  channelIndex = (channelIndex + 1) % CHANNEL_CYCLE_COUNT;
  joined = false; // gate SM sends until CJN confirms the new channel
  Serial.println("Switching to channel " + String(CHANNEL_CYCLE[channelIndex]));
  ws.sendTXT("JC," + String(CHANNEL_CYCLE[channelIndex]));
}

void updateChannelButton() {
  bool raw = digitalRead(PIN_CHANNEL_BUTTON) == LOW;
  unsigned long now = millis();

  if (raw != buttonLastRaw) {
    buttonLastRaw = raw;
    buttonLastChangeMs = now;
    return;
  }

  if (raw != buttonState && now - buttonLastChangeMs >= BUTTON_DEBOUNCE_MS) {
    buttonState = raw;
    if (buttonState) { // fire on press, not release
      cycleChannel();
    }
  }
}

void setup() {
  Serial.begin(115200);
  // Keying line has its own external divider biasing it, so no
  // internal pull-up here -- that would skew the divider math.
  pinMode(PIN_KEY, INPUT);
  pinMode(PIN_CHANNEL_BUTTON, INPUT_PULLUP);

  WiFiManager wm;
  if (!wm.autoConnect(WIFI_MANAGER_AP_NAME)) {
    Serial.println("WiFi provisioning failed, restarting");
    ESP.restart();
  }
  Serial.println("WiFi connected: " + WiFi.localIP().toString());

  ws.begin(VBAND_HOST, VBAND_PORT, VBAND_PATH, VBAND_PROTOCOL);
  ws.onEvent(onWsEvent);
  ws.setReconnectInterval(5000);

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }
  ArduinoOTA.onStart([]() { Serial.println("OTA update starting"); });
  ArduinoOTA.onEnd([]() { Serial.println("OTA update complete"); });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.println("OTA error [" + String(error) + "]");
  });
  ArduinoOTA.begin();
}

void loop() {
  ws.loop();
  updateKey();
  updateChannelButton();
  ArduinoOTA.handle();
}
