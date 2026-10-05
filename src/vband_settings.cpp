#include <Arduino.h>
#include <Preferences.h>
#include "vband_settings.h"
#include "config.h"

namespace {

constexpr const char *NVS_NAMESPACE = "vband";
constexpr const char *KEY_NAME = "name";
constexpr const char *KEY_ROOM = "room";
constexpr const char *KEY_CHANNEL = "channel";

String name;
String room;
int channel = -1;

String sanitize(const String &value) {
  String out = value;
  out.replace(",", "");
  out.trim();
  if (out.length() > VBAND_SETTING_MAX_LEN) {
    out = out.substring(0, VBAND_SETTING_MAX_LEN);
  }
  return out;
}

}  // namespace

void vbandSettingsBegin() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  name = prefs.getString(KEY_NAME, "");
  room = prefs.getString(KEY_ROOM, VBAND_DEFAULT_ROOM);
  channel = prefs.getInt(KEY_CHANNEL, -1);
  if (name.isEmpty()) {
    name = VBAND_DEFAULT_NAME_PREFIX + String(1000 + esp_random() % 9000);
    prefs.putString(KEY_NAME, name);
  }
  prefs.end();
  Serial.println("VBand name: " + name + ", room: " + room + ", channel: " + String(channel));
}

const String &vbandSettingsName() {
  return name;
}

const String &vbandSettingsRoom() {
  return room;
}

int vbandSettingsChannel() {
  return channel;
}

void vbandSettingsSaveChannel(int channelIndex) {
  if (channelIndex == channel) return;
  channel = channelIndex;
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putInt(KEY_CHANNEL, channel);
  prefs.end();
  Serial.println("Saved VBand channel: " + String(channel));
}

void vbandSettingsSave(const String &newName, const String &newRoom) {
  String cleanName = sanitize(newName);
  String cleanRoom = sanitize(newRoom);
  if (!cleanName.isEmpty()) name = cleanName;
  if (!cleanRoom.isEmpty()) room = cleanRoom;

  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString(KEY_NAME, name);
  prefs.putString(KEY_ROOM, room);
  prefs.end();
  Serial.println("Saved VBand name: " + name + ", room: " + room);
}
