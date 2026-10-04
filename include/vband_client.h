#pragma once

// Owns the WebSocket connection to the VBand server: connecting, joining
// the current CHANNEL_CYCLE position, dispatching incoming server
// messages, and sending this device's own keying timing.
void vbandBegin();
void vbandLoop();

// Sends a space/mark pair for this device's own keying. No-ops if not
// currently joined to a channel.
void vbandSendSpaceMark(unsigned long space, unsigned long mark);

// Advances to the next channel in CHANNEL_CYCLE and asks the server to
// join it.
void vbandCycleChannel();

// True once the server has confirmed the join (CJN) for the current
// CHANNEL_CYCLE position; false from the moment a join is requested
// (including on vbandCycleChannel()) until that confirmation arrives.
bool vbandIsJoined();

// True from the first join confirmation after connecting until the
// WebSocket disconnects. Unlike vbandIsJoined(), it stays true while
// switching channels, so a channel change doesn't look like VBand going
// away and back (the keyer would otherwise flip between key lines).
bool vbandIsReady();

// Single-character identifier for the current CHANNEL_CYCLE position,
// for the status LED's Morse announcement: '1'-'5' for the numbered
// channels, 'C' for the custom room (vbandSettingsRoom()).
char vbandChannelCode();

// Name of the current CHANNEL_CYCLE position, as the server knows it
// (e.g. "Channel 5 (ND)" or the custom room).
String vbandChannelName();

// Registers a callback invoked once per inbound SMK from someone else in
// the channel (never for this device's own keying), with who sent it and
// that transmission's original space/mark timing -- e.g. to play it back
// as audio. Call once from setup(); pass nullptr to clear it.
typedef void (*VbandRxSpaceMarkCallback)(const String &userId, const String &userName,
                                         unsigned long space, unsigned long mark);
void vbandSetRxCallback(VbandRxSpaceMarkCallback callback);
