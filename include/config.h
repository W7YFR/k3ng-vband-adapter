#pragma once

// WiFi is provisioned at runtime by WiFiManager (see wifi_setup.cpp),
// not hardcoded here. On first boot, or whenever it can't reconnect, the
// board opens an access point named below -- join it from a phone/laptop
// and a captive portal will ask for your home WiFi's SSID/password, which
// it then remembers in flash.
#define WIFI_MANAGER_AP_NAME "VBand-ESP32"

// Password for joining that access point (WPA2, so 8+ characters --
// enforced in wifi_setup.cpp). Override with WIFI_MANAGER_AP_PASSWORD in
// .env, which is injected at build time like the OTA values below.
#ifndef WIFI_MANAGER_AP_PASSWORD
#define WIFI_MANAGER_AP_PASSWORD "w7yfr-vband"
#endif

// VBand server (reverse-engineered from hamradio.solutions' websockets.js).
// Port 7385 is plaintext ws://; 7386 is wss:// and needs a TLS client.
#define VBAND_HOST "hamradio.solutions"
#define VBAND_PORT 7385
#define VBAND_PATH "/"
#define VBAND_PROTOCOL "lws-hrs-vband2"

// Identity shown to other users, and the channel to join on connect.
#define VBAND_NAME "JIM-BOB"
#define VBAND_CHANNEL "tacos"

// CIRCUIT_TEST (power latch bench test, see circuit_test.cpp)
// When set, the board still connects to WiFi
// and accepts OTA updates, but never joins VBand.
// The button lights the LED while held.
// Run `pio run -e circuit_test -t upload` or set the define below:
// #define CIRCUIT_TEST

// Software debounce to prevent jitter
#define DEBOUNCE_MS 5

// Mechanical pushbuttons bounce longer than the keying line does, so the
// channel button gets its own, looser debounce window.
#define BUTTON_DEBOUNCE_MS 30

// Holding the button this long powers the board off (power_latch.cpp);
// the LED then flashes at POWER_OFF_FLASH_MS on/off until the button is
// released and power drops.
#define POWER_OFF_HOLD_MS 3000
#define POWER_OFF_FLASH_MS 100

// Status LED (led_indicator.cpp). Blink intervals are the on/off
// duration in each state; MORSE_WPM sets the speed of the one-shot
// channel-identifier flash played on every join confirmation.
#define LED_WIFI_DISCONNECTED_BLINK_MS 500
#define LED_WIFI_CONNECTED_BLINK_MS 250
#define MORSE_WPM 10

// Sidetone audio (sidetone.cpp): pitch of the synthesized tone for
// incoming code and the DAC sample rate it's synthesized at (a clean
// divisor of 1,000,000 so the sample timer's period is a whole number
// of microseconds). AUDIO_QUEUE_CAPACITY caps how many received
// space/mark pairs can be buffered awaiting playback -- if playback
// ever falls behind arrival, the oldest queued pair is dropped rather
// than growing unbounded.
#define AUDIO_TONE_HZ 700
#define AUDIO_SAMPLE_RATE_HZ 40000
#define AUDIO_QUEUE_CAPACITY 32

// Over-the-air updates (ArduinoOTA). Hostname is what shows up for
// `pio run -t upload --upload-port <hostname>.local` / Arduino IDE's
// network port list. Both values are normally injected at build time
// from .env (see scripts/hydrate_build_flags.py) so the real device
// hostname/secret never lands in this file; these defaults only apply
// if .env is missing, which leaves OTA unauthenticated on your local
// network under the fallback hostname.
#ifndef OTA_HOSTNAME
#define OTA_HOSTNAME "vband"
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

// The receiving client replays space/mark
// values in real time, one at a time, in order -- an unclamped space (e.g.
// idle time since boot before your first keydown) becomes real playback
// delay for everyone else and backs up everything queued behind it.
#define MAX_TIME_MS 3000
