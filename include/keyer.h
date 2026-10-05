#pragma once

// Non-blocking straight-key reader for PIN_KEY. Reports one space/mark
// pair per key-up: the idle time before the key went down (space) and
// how long it was held (mark), both in milliseconds and clamped to
// MAX_TIME_MS. Edges are timestamped in an interrupt, so the timing is
// exact however late keyerLoop() runs; blips shorter than DEBOUNCE_MS
// are ignored. Reports timing only -- what to do with it (e.g. send it
// somewhere) is the caller's business, not the keyer's.
typedef void (*KeyerSpaceMarkCallback)(unsigned long space, unsigned long mark);

void keyerBegin();
void keyerLoop(KeyerSpaceMarkCallback onSpaceMark);

// True while the key line is active right now (not debounced).
bool keyerKeyDown();
