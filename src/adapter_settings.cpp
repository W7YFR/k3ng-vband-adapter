#include <Arduino.h>
#include <Preferences.h>
#include "adapter_settings.h"
#include "vband_settings.h"
#include "vband_client.h"
#include "sidetone.h"
#include "config.h"

namespace {

struct Definition {
  const char *key;   // also the NVS key and the keyed/CLI name (VB.<key>)
  const char *label; // the keyer's menu row, up to 10 characters
  char type;         // 'b' on/off, 'n' number
  int min, max, step;
  const char *unit;
  int defaultValue;
};

// In AdapterSetting order.
const Definition DEFINITIONS[] = {
    {"LED", "LED keying", 'b', 0, 1, 1, "", LED_FOLLOWS_KEY_DEFAULT},
    {"PITCH", "Pitches", 'b', 0, 1, 1, "", AUDIO_TONE_PER_SENDER_DEFAULT},
    {"TONE", "Tone", 'n', 300, 1200, 10, "Hz", AUDIO_TONE_HZ},
    {"FADE", "Fade", 'n', 0, AUDIO_RAMP_MAX_MS, 1, "ms", AUDIO_RAMP_MS},
    {"WIN", "Switch win", 'n', 3, 30, 1, "s", CHANNEL_SWITCH_WINDOW_S},
};
constexpr int NUMBERS = (int)AdapterSetting::Count;
static_assert(sizeof(DEFINITIONS) / sizeof(DEFINITIONS[0]) == NUMBERS, "one definition per AdapterSetting");

// Text settings, listed first: kept by vband_settings.cpp.
enum Text { Name, Room, TEXTS };
const char *const TEXT_KEYS[TEXTS] = {"NAME", "ROOM"};
const char *const TEXT_LABELS[TEXTS] = {"Name", "Room"};

constexpr const char *NVS_NAMESPACE = "settings";
int values[NUMBERS];

String textValue(int t) {
  return t == Name ? vbandSettingsName() : vbandSettingsRoom();
}

// Settings that take effect somewhere other than where they're read.
void apply(int n) {
  if (n == (int)AdapterSetting::Fade) sidetoneSetFadeMs(values[n]);
}

}  // namespace

void adapterSettingsBegin() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  for (int n = 0; n < NUMBERS; n++) {
    values[n] = constrain(prefs.getInt(DEFINITIONS[n].key, DEFINITIONS[n].defaultValue),
                          DEFINITIONS[n].min, DEFINITIONS[n].max);
  }
  prefs.end();
}

int adapterSetting(AdapterSetting setting) {
  return values[(int)setting];
}

int adapterSettingsCount() {
  return TEXTS + NUMBERS;
}

String adapterSettingDescription(int i) {
  if (i < TEXTS) return String(TEXT_KEYS[i]) + "," + TEXT_LABELS[i] + ",s," + textValue(i);
  const Definition &d = DEFINITIONS[i - TEXTS];
  return String(d.key) + "," + d.label + "," + d.type + "," + values[i - TEXTS] + "," + d.min + "," + d.max +
         "," + d.step + "," + d.unit;
}

int adapterSettingFind(const String &key) {
  for (int t = 0; t < TEXTS; t++) {
    if (key.equalsIgnoreCase(TEXT_KEYS[t])) return t;
  }
  for (int n = 0; n < NUMBERS; n++) {
    if (key.equalsIgnoreCase(DEFINITIONS[n].key)) return TEXTS + n;
  }
  return -1;
}

int adapterSettingSet(const String &key, const String &value) {
  int i = adapterSettingFind(key);
  if (i < 0) return -1;

  if (i < TEXTS) {
    String text = value;
    text.replace(" ", "");
    text.replace(",", "");
    if (text.isEmpty()) return -1;
    if (text == textValue(i)) return i;
    if (i == Name) {
      vbandSettingsSave(text, "");
      vbandReconnect(); // the server only learns our name when we connect
    } else {
      vbandSettingsSave("", text);
      vbandCustomRoomChanged();
    }
    return i;
  }

  int n = i - TEXTS;
  int number = constrain(value.toInt(), DEFINITIONS[n].min, DEFINITIONS[n].max);
  if (number == values[n]) return i;
  values[n] = number;
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putInt(DEFINITIONS[n].key, number);
  prefs.end();
  apply(n);
  Serial.printf("Setting %s = %d\n", DEFINITIONS[n].key, number);
  return i;
}

String adapterSettingValueText(int i) {
  if (i < TEXTS) return textValue(i);
  return String(values[i - TEXTS]);
}
