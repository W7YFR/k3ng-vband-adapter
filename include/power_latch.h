#pragma once

// Drives PIN_POWER_LATCH HIGH to keep the board powered after the user
// releases the power button. Call this first in setup(), before anything
// else, so the latch is asserted as soon as possible after boot. Also
// releases any hold left by powerLatchHoldThroughRestart().
void powerLatchBegin();

// Starts a background task that powers the board off when the button is
// held for POWER_OFF_HOLD_MS (or carries on, if USB keeps it powered). Runs independently of loop(), so it works
// even while setup() is blocked (e.g. in the WiFi config portal). The
// held button keeps the board powered through D1, so it actually shuts
// off when the button is released. Until then it flashes the LED every
// POWER_OFF_FLASH_MS as a "let go now" cue.
void powerOffButtonBegin();

// True once the long press has released the latch. Anything else that
// drives PIN_LED must stop when this is set, so the shutdown flash shows.
bool powerOffPending();

// Locks PIN_POWER_LATCH HIGH at the pad (gpio_hold_en) so it survives a
// software restart -- otherwise the pin floats during reset, Q1 turns off,
// and the board cuts its own power before setup() can re-latch. Call right
// before a deliberate restart (e.g. after an OTA update).
void powerLatchHoldThroughRestart();

// Powers off on request ("/OFF" keyed on the keyer), the same as holding
// the button. Returns only if the board is still powered a moment later
// (over USB): it then stays on, re-latched, and says so on the keyer.
void powerOffNow();

// Drops PIN_POWER_LATCH LOW (clearing any hold), cutting the board's power
// as soon as the button isn't holding it on.
void powerLatchRelease();
