#pragma once

// Drives PIN_POWER_LATCH HIGH to keep the board powered after the user
// releases the power button. Call this first in setup(), before anything
// else, so the latch is asserted as soon as possible after boot. Also
// releases any hold left by powerLatchHoldThroughRestart().
void powerLatchBegin();

// Locks PIN_POWER_LATCH HIGH at the pad (gpio_hold_en) so it survives a
// software restart -- otherwise the pin floats during reset, Q1 turns off,
// and the board cuts its own power before setup() can re-latch. Call right
// before a deliberate restart (e.g. after an OTA update).
void powerLatchHoldThroughRestart();
