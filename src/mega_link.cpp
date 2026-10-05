#include <Arduino.h>
#include "mega_link.h"
#include "pins.h"
#include "config.h"

namespace {

HardwareSerial &megaSerial = Serial2;

MegaLinkFrameCallback frameCallback = nullptr;

bool linkUp = false;
unsigned int badFrames = 0; // since the link last came up -- garbled on the wire?
bool vbandReady = false;
unsigned long lastFrameMs = 0;
unsigned long lastHiMs = 0;

// Bytes after the '$' of the frame being received.
String rxFrame;
bool inFrame = false;

uint8_t checksum(const String &body) {
  uint8_t sum = 0;
  for (size_t i = 0; i < body.length(); i++) sum ^= (uint8_t)body[i];
  return sum;
}

int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Writes "$<body>*XX\n" only if the whole frame fits in the TX buffer
// right now, so a stuck or unpowered keyer can never stall loop().
bool sendFrame(const String &body) {
  char tail[5];
  snprintf(tail, sizeof(tail), "*%02X\n", checksum(body));
  size_t length = 1 + body.length() + 4;
  if (length > MEGA_LINK_MAX_FRAME || megaSerial.availableForWrite() < (int)length) return false;
  megaSerial.write('$');
  megaSerial.print(body);
  megaSerial.print(tail);
  return true;
}

void sendVbandReady() {
  sendFrame(vbandReady ? "VB,1" : "VB,0");
}

void sendHi() {
  sendFrame("HI," MEGA_LINK_PROTOCOL_VERSION);
  sendVbandReady();
  lastHiMs = millis();
}

void setLinkUp(bool up) {
  if (up == linkUp) return;
  linkUp = up;
  if (up) {
    Serial.println("Mega link up");
    badFrames = 0;
  } else {
    Serial.printf("Mega link down (%u bad frames from the keyer since it came up)\n", badFrames);
  }
}

// rxFrame holds "<TYPE>[,<fields>]*XX" -- check it and hand it on.
void handleFrame() {
  int star = rxFrame.lastIndexOf('*');
  int hi = star < 1 ? -1 : hexDigit(rxFrame[star + 1]);
  int lo = star < 1 ? -1 : hexDigit(rxFrame[star + 2]);
  String body = star < 1 ? String() : rxFrame.substring(0, star);
  if (star < 1 || star != (int)rxFrame.length() - 3 || hi < 0 || lo < 0 ||
      checksum(body) != (uint8_t)((hi << 4) | lo)) {
    badFrames++;
    return;
  }

  lastFrameMs = millis();
  if (!linkUp) {
    setLinkUp(true);
    sendHi(); // answer right away instead of waiting for the next heartbeat
  }

  int comma = body.indexOf(',');
  String type = comma == -1 ? body : body.substring(0, comma);
  String fields = comma == -1 ? String() : body.substring(comma + 1);
  if (type == "HI") return;
  if (frameCallback) frameCallback(type, fields);
}

void readFrames() {
  while (megaSerial.available() > 0) {
    char c = (char)megaSerial.read();
    if (c == '$') {
      rxFrame = "";
      inFrame = true;
    } else if (!inFrame || c == '\r') {
      // noise between frames (e.g. the line floating while the keyer is off)
    } else if (c == '\n') {
      handleFrame();
      inFrame = false;
    } else if (rxFrame.length() >= MEGA_LINK_MAX_FRAME) {
      inFrame = false; // too long to be ours; wait for the next '$'
    } else {
      rxFrame += c;
    }
  }
}

}  // namespace

void megaLinkBegin() {
  rxFrame.reserve(MEGA_LINK_MAX_FRAME);
  // Must be set before begin(); without a TX ring buffer, writes wait on
  // the hardware FIFO and availableForWrite() can't be trusted to never block.
  megaSerial.setTxBufferSize(256);
  megaSerial.begin(MEGA_LINK_BAUD, SERIAL_8N1, PIN_MEGA_RX, PIN_MEGA_TX);
  sendHi();
}

void megaLinkLoop() {
  readFrames();

  unsigned long now = millis();
  if (linkUp && now - lastFrameMs > MEGA_LINK_TIMEOUT_MS) setLinkUp(false);

  unsigned long hiInterval = linkUp ? MEGA_LINK_HI_UP_MS : MEGA_LINK_HI_DOWN_MS;
  if (now - lastHiMs >= hiInterval) sendHi();
}

bool megaLinkUp() {
  return linkUp;
}

bool megaLinkSend(const String &type, const String &fields, bool evenIfDown) {
  if (!linkUp && !evenIfDown) return false;
  return sendFrame(fields.length() ? type + "," + fields : type);
}

void megaLinkSetVbandReady(bool ready) {
  if (ready == vbandReady) return;
  vbandReady = ready;
  if (linkUp) sendVbandReady();
}

void megaLinkSendBye(const char *reason) {
  sendFrame(String("BYE,") + reason);
}

void megaLinkSetFrameCallback(MegaLinkFrameCallback callback) {
  frameCallback = callback;
}
