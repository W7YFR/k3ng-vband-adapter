#pragma once

// Non-blocking straight-key reader for PIN_KEY. Reports one space/mark
// pair per key-up: the debounced idle time before the key went down
// (space) and how long it was held (mark), both in milliseconds and
// clamped to MAX_TIME_MS. Reports timing only -- what to do with it
// (e.g. send it somewhere) is the caller's business, not the keyer's.
typedef void (*KeyerSpaceMarkCallback)(unsigned long space, unsigned long mark);

void keyerBegin();
void keyerLoop(KeyerSpaceMarkCallback onSpaceMark);
