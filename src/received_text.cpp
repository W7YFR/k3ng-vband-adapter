#include <Arduino.h>
#include "received_text.h"
#include "morse_decoder.h"
#include "mega_link.h"
#include "vband_client.h"
#include "config.h"

namespace {

struct Sender {
  bool used = false;
  String id;
  String tag;
  MorseDecoder decoder;
  unsigned long lastHeardMs = 0;
};

Sender senders[RX_TEXT_MAX_SENDERS];

// Keeps only what's safe in a frame field and readable on the keyer:
// upper-case letters, digits, '/'.
String cleanTag(const String &raw) {
  String tag;
  for (size_t i = 0; i < raw.length() && tag.length() < RX_TEXT_TAG_MAX_LEN; i++) {
    char c = toupper(raw[i]);
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/') tag += c;
  }
  return tag.length() ? tag : String("?");
}

// The website's findCallsign(): the first word (split on '-' or ' ') at
// least 3 long whose first run of digits is a single digit, e.g. "W7YFR"
// in "W7YFR-RX".
String callsignIn(const String &name) {
  int start = 0;
  while (start <= (int)name.length()) {
    int end = start;
    while (end < (int)name.length() && name[end] != '-' && name[end] != ' ') end++;
    String word = name.substring(start, end);
    int digit = 0;
    while (digit < (int)word.length() && !isdigit(word[digit])) digit++;
    int runEnd = digit;
    while (runEnd < (int)word.length() && isdigit(word[runEnd])) runEnd++;
    if (word.length() >= 3 && runEnd - digit == 1) return word;
    start = end + 1;
  }
  return String();
}

String tagFor(const String &name) {
  String call = callsignIn(name);
  return cleanTag(call.length() ? call : name);
}

bool noDecodeChannel() {
  String channel = vbandChannelName();
  channel.toUpperCase();
  return channel.endsWith("(ND)");
}

// Sends whatever this sender's decoder has completed.
void sendDecoded(Sender &sender) {
  String text = sender.decoder.take();
  if (!text.length()) return;
  if (noDecodeChannel()) {
    // Just who's sending; the keyer starts their line but shows no text.
    text.trim();
    if (!text.length()) return;
    text = "";
  }
  // Even while the keyer hasn't answered lately: dropping text would
  // garble the conversation, and the keyer is normally always there.
  megaLinkSend("RX", sender.tag + "," + text, true);
}

}  // namespace

uint8_t receivedTextSender(const String &userId, const String &userName) {
  int slot = -1;
  for (int i = 0; i < RX_TEXT_MAX_SENDERS; i++) {
    if (senders[i].used && senders[i].id == userId) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    // A new sender takes a free slot, or the one heard from least recently.
    slot = 0;
    for (int i = 0; i < RX_TEXT_MAX_SENDERS; i++) {
      if (!senders[i].used) {
        slot = i;
        break;
      }
      if (senders[i].lastHeardMs < senders[slot].lastHeardMs) slot = i;
    }
    senders[slot] = Sender();
    senders[slot].used = true;
    senders[slot].id = userId;
  }
  senders[slot].tag = tagFor(userName); // names can change mid-session
  senders[slot].lastHeardMs = millis();
  return slot;
}

void receivedTextPlayed(uint8_t sender, unsigned long space, unsigned long mark) {
  if (sender >= RX_TEXT_MAX_SENDERS || !senders[sender].used) return;
  senders[sender].decoder.add(space, mark);
  sendDecoded(senders[sender]);
}

void receivedTextLoop() {
  for (Sender &sender : senders) {
    if (!sender.used) continue;
    sender.decoder.flushIfIdle();
    sendDecoded(sender);
  }
}
