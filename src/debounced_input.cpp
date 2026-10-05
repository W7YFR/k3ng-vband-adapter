#include "debounced_input.h"

DebouncedInput::DebouncedInput(uint8_t pin, unsigned long debounceMs, bool activeHigh)
    : pin_(pin), debounceMs_(debounceMs), activeHigh_(activeHigh) {}

void DebouncedInput::begin(uint8_t mode) {
  pinMode(pin_, mode);
}

bool DebouncedInput::update() {
  bool raw = digitalRead(pin_) == (activeHigh_ ? HIGH : LOW);
  unsigned long now = millis();

  if (raw != lastRaw_) {
    lastRaw_ = raw;
    lastChangeMs_ = now;
    return false;
  }

  if (raw != state_ && now - lastChangeMs_ >= debounceMs_) {
    state_ = raw;
    return true;
  }

  return false;
}

bool DebouncedInput::activeNow() const {
  return state_;
}
