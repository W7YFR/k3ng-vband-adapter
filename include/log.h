#pragma once

#include <Arduino.h>

// The adapter's log: use Log.print / println / printf where you'd use
// Serial's. Everything goes to the USB serial port as before, and is also
// kept (the last LOG_HISTORY_LINES lines) so it can be forwarded to the
// keyer over the link, which prints it on the keyer's USB port: the only
// port you can reach with the adapter in its case. Safe to call from any
// task.
class LogOutput : public Print {
 public:
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buffer, size_t size) override;
};

extern LogOutput Log;

// VB.LOG: forward the log to the keyer. Turning it on starts with the
// lines already kept, so you see what happened before you plugged in.
void logForwardSet(bool on);

// Sends the next waiting line (or part of one) to the keyer, from loop().
// Frames: "LG,<text>" ends a line, "LGP,<text>" is part of a longer one.
void logForwardLoop();
