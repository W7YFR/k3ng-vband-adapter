#pragma once

// Carries the keying line through an external voltage divider
// (plain INPUT, biased externally -- see setup() in main.cpp).
// GPIO16 is silkscreened "D4" on the Wemos D1 Mini32; don't confuse it
// with GPIO4, which is a different, unrelated pin on this board.
#define PIN_KEY 16

// Momentary pushbutton to GND, internal pull-up enabled (INPUT_PULLUP)
// -- unlike PIN_KEY, this one has no external biasing. Cycles through
// CHANNEL_CYCLE on each press (see main.cpp).
#define PIN_CHANNEL_BUTTON 17
