#pragma once

#include <Arduino.h>

// Two-way serial link to the K3NG keyer (Arduino Mega), which shows our
// status and received VBand traffic on its display and sends us commands
// keyed in its command mode. Each message is one line:
//
//   $<TYPE>[,<fields>]*<XX>\n     XX = 2 hex digits, XOR of the bytes between $ and *
//
// Malformed lines (no $, bad checksum, longer than MEGA_LINK_MAX_FRAME)
// are dropped. Either board may be powered off at any time, so this never
// blocks: we keep sending HI as a heartbeat, the link is "up" while valid
// frames keep arriving from the keyer, and anything else sent while it's
// down (or that doesn't fit in the UART's TX buffer) is dropped, not queued.
typedef void (*MegaLinkFrameCallback)(const String &type, const String &fields);

void megaLinkBegin();
void megaLinkLoop();

// True while the keyer has been heard from in the last MEGA_LINK_TIMEOUT_MS.
bool megaLinkUp();

// Sends one frame; returns false if it was dropped (link down or no room).
// evenIfDown sends it while the link is down too (bench testing).
bool megaLinkSend(const String &type, const String &fields, bool evenIfDown = false);

// Tells the keyer whether VBand is usable right now, as "VB,1" / "VB,0".
// Sent on every change and again with every heartbeat, so the keyer
// catches up even if a frame was lost. The keyer only moves keying to
// VBand while this is true.
void megaLinkSetVbandReady(bool ready);

// Tells the keyer we're about to go away on purpose (reboot or power
// off), so it switches back right away instead of waiting out the link
// timeout. Called from the power-off button task as well as loop(); a
// frame mangled by the two writing at once just fails the keyer's checksum.
void megaLinkSendBye(const char *reason);

// Called for every valid frame from the keyer other than HI.
void megaLinkSetFrameCallback(MegaLinkFrameCallback callback);
