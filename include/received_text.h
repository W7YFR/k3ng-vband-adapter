#pragma once

#include <Arduino.h>

// Turns other users' VBand keying into text on the keyer's display, as
// "RX,<tag>,<text>" frames over mega_link. Each sender gets their own
// MorseDecoder (it adapts to their speed) and a short tag the keyer puts
// at the start of a new line whenever the sender changes: their callsign
// if their VBand name has one (as the website finds it), otherwise the
// start of their name.
//
// Decoding happens as the sidetone plays each element, not as it
// arrives, so the text keeps pace with what you hear. On no-decode
// channels ("... (ND)") only the tag is sent, never the text.

// Slot for this sender, to queue with their timings for playback.
uint8_t receivedTextSender(const String &userId, const String &userName);

// The sidetone finished playing one of this sender's space/mark pairs.
void receivedTextPlayed(uint8_t sender, unsigned long space, unsigned long mark);

// Finishes characters left hanging at the end of a transmission. Call
// every loop().
void receivedTextLoop();
