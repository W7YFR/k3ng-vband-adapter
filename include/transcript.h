#pragma once

#include <Arduino.h>

// The VBand session as a transcript: who sent what (us included), joins
// and leaves, room changes, connects and drops, each line starting with
// the UTC time from NTP, e.g. "[14:02Z] W7YFR: CQ CQ" or
// "[14:03Z] * K1ABC joined (Bob K1ABC)".
//
// A message is one sender's sending until they've been quiet for
// TRANSCRIPT_MESSAGE_GAP_MS. Two ways out:
// - Live (VB.UI on): sent to the keyer as it's decoded ("TRP,<text>"
//   continues the current line, "TR,<text>" ends it), which prints it on
//   its USB port. When a sender interrupted by someone else goes on, their
//   new line repeats their whole message so far, so every line reads whole.
// - Recorded, always: each finished message as one line, in the order the
//   messages started, the last TRANSCRIPT_RECORD_BYTES of them, in RAM.
//
// "UI,1" / "UI,0" tell the keyer whether the live stream is on (on change
// and every couple of seconds), so it can stop echoing our own sending to
// its USB port while the transcript carries it.

// Starts NTP; call once WiFi has been set up.
void transcriptBegin();
void transcriptLoop();

// Text decoded from someone's sending ("" on no-decode channels: the
// line then shows just who's sending).
void transcriptText(const String &userId, const String &tag, const String &text);

// Our own keying, as it goes to VBand: decoded here for the transcript.
void transcriptOwnSpaceMark(unsigned long space, unsigned long mark);

// A line of its own: "* <text>".
void transcriptNotice(const String &text);

// What the recording holds, for /DIAG.
uint32_t transcriptRecordedLines();
size_t transcriptRecordedBytes();
