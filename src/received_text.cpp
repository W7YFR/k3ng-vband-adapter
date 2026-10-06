#include <Arduino.h>
#include "received_text.h"
#include "morse_decoder.h"
#include "mega_link.h"
#include "vband_client.h"
#include "user_tag.h"
#include "config.h"
#include "adapter_settings.h"

namespace {

struct Sender {
  bool used = false;
  String id;
  String tag;
  MorseDecoder decoder;
  unsigned long lastHeardMs = 0;
};

Sender senders[RX_TEXT_MAX_SENDERS];

bool noDecodeChannel() {
  String channel = vbandChannelName();
  channel.toUpperCase();
  return channel.endsWith("(ND)");
}

// Sends whatever this sender's decoder has completed.
void sendDecoded(Sender &sender) {
  String text = sender.decoder.take();
  if (!text.length()) return;
  int show = adapterSetting(AdapterSetting::Rx);
  if (show == RX_SHOW_NONE) return; // audio only
  if (show == RX_SHOW_SENDER || noDecodeChannel()) {
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
  senders[slot].tag = userTag(userName); // names can change mid-session
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
