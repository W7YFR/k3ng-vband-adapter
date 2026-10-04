#include <Arduino.h>
#include "user_tag.h"
#include "config.h"

namespace {

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

}  // namespace

String userTag(const String &name) {
  String call = callsignIn(name);
  return cleanTag(call.length() ? call : name);
}
