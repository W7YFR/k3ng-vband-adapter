#pragma once

// Drives PIN_POWER_LATCH HIGH to keep the board powered after the user
// releases the power button. Call this first in setup(), before anything
// else, so the latch is asserted as soon as possible after boot.
void powerLatchBegin();
