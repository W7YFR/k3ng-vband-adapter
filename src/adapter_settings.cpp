#include <Arduino.h>
#include <Preferences.h>
#include "adapter_settings.h"
#include "vband_settings.h"
#include "vband_client.h"
#include "sidetone.h"
#include "ota_updater.h"
#include "config.h"

namespace {

struct Definition {
  const char *key;   // also the NVS key and the keyed/CLI name (VB.<key>)
  const char *label; // the keyer's menu row, up to 10 characters
  char type;         // 'b' on/off, 'n' number, 'e' one of the options
  int min, max, step;
  const char *unit;  // for 'e', the options' names, |-separated
  int defaultValue;
};

// In AdapterSetting order.
const Definition DEFINITIONS[] = {
    {"START", "On startup", 'e', 0, 3, 1, "Off|Lobby|Last|Custom", VBAND_START_DEFAULT},
    {"OTA", "OTA", 'e', 0, 2, 1, "Off|On|10min", OTA_MODE_DEFAULT},
    {"RX", "Received", 'e', 0, 2, 1, "Text|Sender|Off", RX_SHOW_DEFAULT},
    {"JOIN", "Joins", 'b', 0, 1, 1, "", JOIN_NOTICES_DEFAULT},
    {"VOL", "Volume", 'n', 0, 100, 5, "%", AUDIO_VOLUME_DEFAULT},
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
  if (n == (int)AdapterSetting::Volume) sidetoneSetVolume(values[n]);
  if (n == (int)AdapterSetting::Ota) otaSetMode(values[n]);
}

// An option's index from its number or (case-insensitive) name, or -1.
int optionIndex(const Definition &d, const String &value) {
  if (value.length() && isDigit(value[0])) return value.toInt();
  int index = 0;
  int start = 0;
  String options = d.unit;
  while (start <= (int)options.length()) {
    int end = options.indexOf('|', start);
    if (end < 0) end = options.length();
    if (value.equalsIgnoreCase(options.substring(start, end))) return index;
    index++;
    start = end + 1;
  }
  return -1;
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
  if (values[(int)AdapterSetting::Ota] == OTA_MODE_WINDOW) values[(int)AdapterSetting::Ota] = OTA_MODE_OFF;
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
      vbandNameChanged();
    } else {
      vbandSettingsSave("", text);
      vbandCustomRoomChanged();
    }
    return i;
  }

  int n = i - TEXTS;
  int number = DEFINITIONS[n].type == 'e' ? optionIndex(DEFINITIONS[n], value) : value.toInt();
  if (DEFINITIONS[n].type == 'e' && number < 0) return -1;
  number = constrain(number, DEFINITIONS[n].min, DEFINITIONS[n].max);
  if (number == values[n] && n != (int)AdapterSetting::Ota) return i; // OTA: choosing 10min again restarts it
  values[n] = number;
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  // An OTA window is for now; after a reboot it's off.
  prefs.putInt(DEFINITIONS[n].key, (n == (int)AdapterSetting::Ota && number == OTA_MODE_WINDOW) ? OTA_MODE_OFF : number);
  prefs.end();
  apply(n);
  Serial.printf("Setting %s = %d\n", DEFINITIONS[n].key, number);
  return i;
}

void adapterSettingOtaWindowEnded() {
  values[(int)AdapterSetting::Ota] = OTA_MODE_OFF;
}

String adapterSettingValueText(int i) {
  if (i < TEXTS) return textValue(i);
  return String(values[i - TEXTS]);
}
