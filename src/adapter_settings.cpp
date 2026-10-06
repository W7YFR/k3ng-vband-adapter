#include <Arduino.h>
#include <Preferences.h>
#include "adapter_settings.h"
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

const Definition DEFINITIONS[] = {
    {"LED", "LED keying", 'b', 0, 1, 1, "", LED_FOLLOWS_KEY_DEFAULT},
    {"PITCH", "Pitches", 'b', 0, 1, 1, "", AUDIO_TONE_PER_SENDER_DEFAULT},
    {"FADE", "Fade", 'n', 0, AUDIO_RAMP_MAX_MS, 1, "ms", AUDIO_RAMP_MS},
    {"WIN", "Switch win", 'n', 3, 30, 1, "s", CHANNEL_SWITCH_WINDOW_S},
};
static_assert(sizeof(DEFINITIONS) / sizeof(DEFINITIONS[0]) == (int)AdapterSetting::Count,
              "one definition per AdapterSetting");

constexpr int COUNT = (int)AdapterSetting::Count;
constexpr const char *NVS_NAMESPACE = "settings";
int values[COUNT];

// Settings that take effect somewhere other than where they're read.
void apply(int i) {
  if (i == (int)AdapterSetting::Fade) sidetoneSetFadeMs(values[i]);
}

}  // namespace

void adapterSettingsBegin() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  for (int i = 0; i < COUNT; i++) {
    values[i] = constrain(prefs.getInt(DEFINITIONS[i].key, DEFINITIONS[i].defaultValue),
                          DEFINITIONS[i].min, DEFINITIONS[i].max);
  }
  prefs.end();
}

int adapterSetting(AdapterSetting setting) {
  return values[(int)setting];
}

int adapterSettingsCount() {
  return COUNT;
}

String adapterSettingDescription(int i) {
  const Definition &d = DEFINITIONS[i];
  return String(d.key) + "," + d.label + "," + d.type + "," + values[i] + "," + d.min + "," + d.max +
         "," + d.step + "," + d.unit;
}

int adapterSettingFind(const String &key) {
  for (int i = 0; i < COUNT; i++) {
    if (key.equalsIgnoreCase(DEFINITIONS[i].key)) return i;
  }
  return -1;
}

int adapterSettingSet(const String &key, int value) {
  int i = adapterSettingFind(key);
  if (i < 0) return -1;
  value = constrain(value, DEFINITIONS[i].min, DEFINITIONS[i].max);
  if (value == values[i]) return i;
  values[i] = value;
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putInt(DEFINITIONS[i].key, value);
  prefs.end();
  apply(i);
  Serial.printf("Setting %s = %d\n", DEFINITIONS[i].key, value);
  return i;
}

int adapterSettingValueAt(int i) {
  return values[i];
}

const char *adapterSettingKeyAt(int i) {
  return DEFINITIONS[i].key;
}
