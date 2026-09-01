#pragma once

#include <Arduino.h>

// Non-blocking debounce for a single active-low digital input pin. Two
// consecutive stable reads spaced at least debounceMs apart are required
// before a transition is reported, so electrical bounce on press/release
// doesn't register as multiple transitions.
class DebouncedInput {
 public:
  DebouncedInput(uint8_t pin, unsigned long debounceMs);

  // Configures the pin with the given mode (INPUT, INPUT_PULLUP, ...).
  void begin(uint8_t mode);

  // Call every loop(). Returns true exactly once when the debounced
  // state changes; check activeNow() to see which way it changed.
  bool update();

  // True when the pin currently reads LOW, after debouncing.
  bool activeNow() const;

 private:
  uint8_t pin_;
  unsigned long debounceMs_;
  bool lastRaw_ = false;
  bool state_ = false;
  unsigned long lastChangeMs_ = 0;
};
