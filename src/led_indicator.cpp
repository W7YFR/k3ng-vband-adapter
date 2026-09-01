#include <Arduino.h>
#include "led_indicator.h"
#include "wifi_setup.h"
#include "vband_client.h"
#include "pins.h"
#include "config.h"

namespace {

constexpr unsigned long DIT_MS = 1200 / MORSE_WPM;
constexpr unsigned long DAH_MS = DIT_MS * 3;
constexpr unsigned long SYMBOL_GAP_MS = DIT_MS;

const char *morsePatternFor(char code) {
  switch (code) {
    case '1': return ".----";
    case '2': return "..---";
    case '3': return "...--";
    case '4': return "....-";
    case '5': return ".....";
    case 'C': return "-.-.";
    default: return "";
  }
}

enum class Mode { WifiDisconnected, AwaitingJoin, Idle, Announcing };

Mode mode = Mode::WifiDisconnected;
bool ledOn = false;
unsigned long phaseStartMs = 0;
bool wasJoined = false;

// Morse playback cursor, valid only while mode == Announcing.
const char *pattern = "";
int symbolIndex = 0;
bool inSymbolGap = false;

void setLed(bool on) {
  if (on == ledOn) return;
  ledOn = on;
  digitalWrite(PIN_LED, on ? HIGH : LOW);
}

void startAnnounce(char code) {
  pattern = morsePatternFor(code);
  symbolIndex = 0;
  inSymbolGap = false;
  mode = Mode::Announcing;
  phaseStartMs = millis();
  setLed(pattern[0] != '\0');
}

void updateBlink(unsigned long intervalMs) {
  unsigned long now = millis();
  if (now - phaseStartMs >= intervalMs) {
    phaseStartMs = now;
    setLed(!ledOn);
  }
}

void updateAnnounce() {
  if (pattern[symbolIndex] == '\0') {
    setLed(false);
    mode = Mode::Idle;
    return;
  }

  unsigned long duration =
      inSymbolGap ? SYMBOL_GAP_MS : (pattern[symbolIndex] == '-' ? DAH_MS : DIT_MS);
  if (millis() - phaseStartMs < duration) return;

  phaseStartMs = millis();
  if (!inSymbolGap) {
    inSymbolGap = true;
    setLed(false);
    return;
  }

  inSymbolGap = false;
  symbolIndex++;
  if (pattern[symbolIndex] == '\0') {
    setLed(false);
    mode = Mode::Idle;
  } else {
    setLed(true);
  }
}

}  // namespace

void ledBegin() {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  mode = Mode::WifiDisconnected;
  phaseStartMs = millis();
}

void ledLoop() {
  bool wifiUp = wifiConnected();
  bool joinedNow = wifiUp && vbandIsJoined();

  if (!wifiUp) {
    mode = Mode::WifiDisconnected;
    wasJoined = false;
  } else if (mode != Mode::Announcing) {
    if (!joinedNow) {
      if (mode != Mode::AwaitingJoin) {
        mode = Mode::AwaitingJoin;
        phaseStartMs = millis();
        setLed(false);
      }
      wasJoined = false;
    } else if (!wasJoined) {
      wasJoined = true; // rising edge: just (re)joined -- announce it
      startAnnounce(vbandChannelCode());
    } else if (mode != Mode::Idle) {
      mode = Mode::Idle;
      setLed(false);
    }
  }

  switch (mode) {
    case Mode::WifiDisconnected:
      updateBlink(LED_WIFI_DISCONNECTED_BLINK_MS);
      break;
    case Mode::AwaitingJoin:
      updateBlink(LED_WIFI_CONNECTED_BLINK_MS);
      break;
    case Mode::Announcing:
      updateAnnounce();
      break;
    case Mode::Idle:
      break;
  }
}
