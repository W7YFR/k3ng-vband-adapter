#pragma once

// One-shot LED patterns shared by modules that drive PIN_LED while loop()
// isn't running (WiFi connect, OTA). Blocking; each is a no-op once a
// power-off is pending, since power_latch.cpp owns the LED then.

// LED_SUCCESS_FLASH_COUNT flashes of LED_SUCCESS_FLASH_MS on/off, ending
// with the LED off. Used for WiFi connected and OTA upload succeeded.
void ledFlashSuccess();
