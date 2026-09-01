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
