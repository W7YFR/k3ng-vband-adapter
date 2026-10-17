#pragma once

#include <Arduino.h>

// Adaptive Morse decoder for one sender's space/mark timings, ported from
// VBand's own decoder.js so text comes out the way the website shows it.
// It learns the sender's dit length and character spacing as it goes.
//
// Feed it each received space/mark pair in order, then take() whatever
// text that completed. A character only completes once the following
// space shows it's over -- or, at the end of a transmission, once
// flushIfIdle() sees no mark for a while.
class MorseDecoder {
 public:
  void add(unsigned long space, unsigned long mark);

  // Completes a character left hanging at the end of a transmission.
  // Call every loop().
  void flushIfIdle();

  // Completes the character now: for keying timed right here, once the key
  // is known to have been up long enough (flushIfIdle() can't tell, since
  // a mark only arrives once it's over).
  void endCharacter() { flush(); }

  // The sender's dit length as learned so far, in ms.
  float ditLength() const { return ditLength_; }

  // Text decoded since the last call: letters, digits, punctuation, ' '
  // between words, '*' for anything unrecognized, and prosigns as their
  // two letters (e.g. "SK", "BT").
  String take();

 private:
  void decodeSpace(unsigned long duration);
  void decodeMark(unsigned long duration);
  void flush();

  float ditLength_ = 1200.0f / 15; // 15 WPM to start
  float lastMark_ = 120;
  float charSpace_ = 3 * 1200.0f / 80;
  uint16_t morseChar_ = 1; // elements so far as binary, after a leading 1; dah = 1
  unsigned long flushAtMs_ = 0; // 0 = nothing pending
  String text_;
};
