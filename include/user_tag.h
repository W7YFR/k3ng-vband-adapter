#pragma once

#include <Arduino.h>

// Short tag for a VBand user, as shown on the keyer: the callsign in
// their name if it has one (found the way the website does, e.g. "W7YFR"
// in "W7YFR-RX"), otherwise the start of the name. Upper case, at most
// RX_TEXT_TAG_MAX_LEN characters of A-Z, 0-9 and '/', so it's safe in a
// link frame.
String userTag(const String &name);
