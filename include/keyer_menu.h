#pragma once

#include <Arduino.h>

// The VBand part of the keyer's settings menu, run here: the keyer sends
// each key pressed in it and draws the screen we send back, so a new
// setting or command shows up with only this firmware changed. The keyer
// holds just what's on screen.
//
// Keyer to us:
//   MK,O,<columns>,<rows>  VBand opened (rows counts the title line)
//   MK,<key>               E down (or the next option / a lower value
//                          while one is being changed), T up, R open /
//                          toggle / run / save, B back or cancel
//   MK,P                   send the whole screen again
//   MK,X                   the keyer left the menu (X, its command button)
//   MW,<text>              the word keyed for a text setting ("" cancels)
// Us to the keyer:
//   MH,<beep>,<title>      the top line: the page's title, or a note until
//                          the next key (beep 1: sound the error beep)
//   MR,<row>,<line>        a row under it (1 = the first), drawn in full
//                          with the '>' or '*' mark in its first column
//   MC,<row>,<editing>     the mark is on that row now ('*' if editing);
//                          every key is answered with at least this
//   MI,<label>             key a word for this text setting (then MW)
//   MA                     a command runs: its answer shows next, then the
//                          keyer asks for the screen again (MK,P)
//   MX                     backed out of VBand: the keyer's own menu again
//
// The rules are the keyer's own menu's: row 0 is "< Back"; a list opens on
// row 1, with "< Back" scrolled off the top when the rest fills the
// screen; options wrap around; only the one of Connect / Disconnect that
// applies is shown; nothing here transmits.

// Handles the frames above; false for any other.
bool keyerMenuFrame(const String &type, const String &fields);

// Sends what's changed on the keyer's screen, a frame at a time.
void keyerMenuLoop();
