#pragma once

// Carries the keying line through an external voltage divider
// (plain INPUT, biased externally -- see keyerBegin() in keyer.cpp).
// GPIO16 is silkscreened "D4" on the Wemos D1 Mini32; don't confuse it
// with GPIO4, which is a different, unrelated pin on this board.
#define PIN_KEY 16

// Momentary pushbutton to GND, internal pull-up enabled (INPUT_PULLUP)
// -- unlike PIN_KEY, this one has no external biasing. Cycles through
// CHANNEL_CYCLE on each press (see channel_button.cpp and vband_client.cpp).
#define PIN_CHANNEL_BUTTON 17

// Status LED, active-high through a current-limiting resistor to GND.
// See led_indicator.cpp for the blink/Morse patterns it drives.
#define PIN_LED 15

// Sidetone audio out -- one of the ESP32's two built-in 8-bit DACs
// (GPIO25/DAC1, GPIO26/DAC2 are the only two valid pins for dacWrite()).
// Feeds into the same amplifier/speaker circuit the existing keyer's own
// sidetone output drives -- not into the keyer port/pin. Idles at ~1.65V
// (mid-scale); AC-couple through a series capacitor so that DC bias
// doesn't reach the amp, and sum the two sources into the amp's input
// through separate resistors (don't tie the two outputs directly
// together) so neither one loads or fights the other. Also pad down
// with that summing resistor since DAC full-scale (3.3V) will be far
// hotter than a typical sidetone line level. See sidetone.cpp.
#define PIN_AUDIO_OUT 25
