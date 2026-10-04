#include <Arduino.h>
#include "morse_decoder.h"
#include "config.h"

namespace {

// decoder.js's morse_table, indexed by the element pattern as binary after
// a leading 1 (dit = 0, dah = 1): "E" (.) is 0b10 = 2, "T" (-) is 0b11 = 3.
// Its accented letters are '*' here, since the keyer's font is plain
// ASCII. Prosign stand-ins: & AS, + AR, = BT, ^ KA, ( KN, | SK.
const char MORSE_TABLE[] =
    "**ETIANMSURWDKGOHVF*L*PJBXCYZQ**54*3***2&*+****16=/**^(*7***8*90"
    "*****|******?_****\"**.****@***'**-********;!*)*****,****:*******";

// decoder.js's two patterns too long for the table.
const uint8_t MORSE_DOLLAR = 0x89; // ...-..-
const uint8_t MORSE_BK = 0xc5;     // -...-.-

const char *spellOut(char c) {
  switch (c) {
    case '&': return "AS";
    case '+': return "AR";
    case '=': return "BT";
    case '^': return "KA";
    case '(': return "KN";
    case '|': return "SK";
    case '~': return "BK";
    default: return nullptr;
  }
}

}  // namespace

void MorseDecoder::add(unsigned long space, unsigned long mark) {
  decodeSpace(space);
  decodeMark(mark);
}

void MorseDecoder::flushIfIdle() {
  if (flushAtMs_ && (long)(millis() - flushAtMs_) >= 0) flush();
}

String MorseDecoder::take() {
  String out = text_;
  text_ = "";
  return out;
}

void MorseDecoder::flush() {
  flushAtMs_ = 0;
  if (morseChar_ > 1) {
    char c = '*';
    if (morseChar_ == MORSE_DOLLAR) {
      c = '$';
    } else if (morseChar_ == MORSE_BK) {
      c = '~';
    } else if (morseChar_ < sizeof(MORSE_TABLE) - 1) {
      c = MORSE_TABLE[morseChar_];
    }
    const char *prosign = spellOut(c);
    if (prosign) {
      text_ += prosign;
    } else {
      text_ += c;
    }
  }
  morseChar_ = 1;
}

void MorseDecoder::decodeSpace(unsigned long duration) {
  flushAtMs_ = 0;
  if (duration <= ditLength_ * 2) return; // between elements of one character

  flush();
  float wordSpace = charSpace_ / 3 * 5.5f;
  if (duration >= wordSpace || duration >= MAX_TIME_MS) {
    text_ += ' ';
    // Slow the learned character spacing a little after each word space.
    charSpace_ *= 1.03f;
  } else if (duration < charSpace_) {
    charSpace_ = charSpace_ * 0.5f + duration * 0.5f; // approach shorter spacing quickly
  } else {
    charSpace_ = charSpace_ * 0.8f + duration * 0.2f;
  }
}

void MorseDecoder::decodeMark(unsigned long duration) {
  // Past 7 elements nothing matches anyway (it'll come out as '*'); stop
  // shifting before the pattern overflows into something that would.
  if (morseChar_ < 0x4000) {
    morseChar_ *= 2;
    if (duration > ditLength_ * 1.7f) morseChar_ += 1;
  }

  // Learn the speed: whenever this mark and the last differ by more than
  // 2x (a dit next to a dah), their average over 2 is a dit length.
  float mark = duration;
  if (mark > 2 * lastMark_ || lastMark_ > 2 * mark) {
    ditLength_ = ((lastMark_ + mark) / 4 + ditLength_) / 2;
    if (charSpace_ < ditLength_ * 2.5f) charSpace_ = ditLength_ * 2.5f;
  }
  lastMark_ = mark;

  // If no mark follows, finish the character after a character space
  // plus a generous allowance.
  flushAtMs_ = millis() + (unsigned long)(ditLength_ * 8);
  if (flushAtMs_ == 0) flushAtMs_ = 1;
}
