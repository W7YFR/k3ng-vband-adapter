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

// Keep holding the power-on press this long after boot to open the config
// portal even when WiFi is already set up (e.g. to change the VBand name
// or room). While booting, the LED is solid as long as that press is held,
// then blinks at LED_WIFI_CONNECTING_BLINK_MS on/off while connecting to
// WiFi, or LED_PORTAL_BLINK_MS while the portal is open.
#define PORTAL_HOLD_MS 2000
#define LED_WIFI_CONNECTING_BLINK_MS 500
#define LED_PORTAL_BLINK_MS 1000

// VBand server (reverse-engineered from hamradio.solutions' websockets.js).
// Port 7385 is plaintext ws://; 7386 is wss:// and needs a TLS client.
#define VBAND_HOST "hamradio.solutions"
#define VBAND_PORT 7385
#define VBAND_PATH "/"
#define VBAND_PROTOCOL "lws-hrs-vband2"

// Defaults for the user name and custom room, both editable in the WiFi
// config portal and persisted in NVS (see vband_settings.cpp). The name
// gets a random number appended once, on first boot.
#define VBAND_DEFAULT_NAME_PREFIX "jim-bob-"
#define VBAND_DEFAULT_ROOM "tacos"
#define VBAND_SETTING_MAX_LEN 32

// CIRCUIT_TEST (bench test of the power latch, key line and keyer link, see circuit_test.cpp)
// When set, the board still connects to WiFi
// and accepts OTA updates, but never joins VBand.
// The LED lights while the button is held or the key line is active,
// and each button press sends the keyer a test status screen.
// Run `pio run -e circuit_test -t upload` or set the define below:
// #define CIRCUIT_TEST

// Software debounce to prevent jitter
#define DEBOUNCE_MS 5

// The keying line idles LOW and goes HIGH while the key is down (the
// keyer's dedicated tx_key_line_2 through a divider -- see pins.h).
#define KEY_ACTIVE_HIGH true

// Mechanical pushbuttons bounce longer than the keying line does, so the
// channel button gets its own, looser debounce window.
#define BUTTON_DEBOUNCE_MS 30

// Holding the button this long powers the board off (power_latch.cpp);
// the LED then flashes at POWER_OFF_FLASH_MS on/off until the button is
// released and power drops.
#define POWER_OFF_HOLD_MS 2000
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
#define AUDIO_SAMPLE_RATE_HZ 20000
#define AUDIO_QUEUE_CAPACITY 32

// Received text (received_text.cpp): how many recent senders get their
// own decoder at once (beyond that, the one heard from least recently is
// reused), and the longest tag shown for a sender on the keyer.
#define RX_TEXT_MAX_SENDERS 8
#define RX_TEXT_TAG_MAX_LEN 7

// Who's in the room (room_users.cpp). After joining a channel the join
// screen waits up to JOIN_SCREEN_WAIT_MS for the server's user list so it
// can include it. The list is asked for again whenever the server says
// the channel's count changed, and every ROOM_USERS_POLL_MS regardless in
// case it doesn't.
#define JOIN_SCREEN_WAIT_MS 2000
#define ROOM_USERS_POLL_MS 30000

// Listen for OTA updates the whole time the board is running. Comment
// out to turn OTA off (it can then only be flashed over USB).
#define OTA_ALWAYS_ON

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

// Success flash (led_patterns.cpp): LED_SUCCESS_FLASH_COUNT flashes of
// LED_SUCCESS_FLASH_MS on/off, shown when WiFi connects at boot and right
// before the reboot after a successful OTA upload.
#define LED_SUCCESS_FLASH_COUNT 3
#define LED_SUCCESS_FLASH_MS 150

// Other OTA LED patterns (ota_updater.cpp): a fast flicker at
// OTA_PROGRESS_FLASH_MS on/off while the upload is in progress (it freezes
// if the upload stalls); on failure, one long OTA_FAILURE_FLASH_MS flash,
// after which the old firmware carries on.
#define OTA_PROGRESS_FLASH_MS 50
#define OTA_FAILURE_FLASH_MS 1000

// The receiving client replays space/mark
// values in real time, one at a time, in order -- an unclamped space (e.g.
// idle time since boot before your first keydown) becomes real playback
// delay for everyone else and backs up everything queued behind it.
#define MAX_TIME_MS 3000

// Serial link to the K3NG keyer (mega_link.cpp). 38400 baud keeps the
// Mega's 16MHz UART clock error around 0.2% (vs ~2% at 115200). The
// ESP32 always talks first: it sends HI every MEGA_LINK_HI_DOWN_MS until
// the keyer answers, then every MEGA_LINK_HI_UP_MS as a heartbeat. The
// link counts as down after MEGA_LINK_TIMEOUT_MS without a valid frame
// from the keyer. MEGA_LINK_MAX_FRAME caps one "$...*XX" frame in bytes.
#define MEGA_LINK_BAUD 38400
#define MEGA_LINK_HI_DOWN_MS 1000
#define MEGA_LINK_HI_UP_MS 2000
#define MEGA_LINK_TIMEOUT_MS 6000
#define MEGA_LINK_MAX_FRAME 64
#define MEGA_LINK_PROTOCOL_VERSION "1"

// Columns on the keyer's display (its LCD_COLUMNS). Status screens are
// cut to this width before sending, to keep frames short.
#define KEYER_DISPLAY_COLUMNS 18
