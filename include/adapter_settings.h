#pragma once

#include <Arduino.h>

// Settings the keyer can list, read and change over the link -- from its
// command-mode menu, keyed shortcuts ("/VB LED OFF") and its CLI ("\$
// vb.led off"). Saved in NVS, so they survive reboots and reflashing;
// the defaults are in config.h.
//
// The list also carries the commands (keyer_commands.cpp) as actions, so
// the keyer's menu shows everything in one place.

enum class AdapterSetting { Led, Pitch, Fade, Window, Count };

void adapterSettingsBegin();

int adapterSetting(AdapterSetting setting);

// For the link (keyer_commands.cpp).
int adapterSettingsCount();
// "<key>,<label>,<type>,<value>,<min>,<max>,<step>,<unit>" for setting i:
// type b (on/off, value 0/1) or n (number).
String adapterSettingDescription(int i);
// Sets a setting by key (case-insensitive), clamping it to its range, and
// saves it. Returns the index, or -1 if there's no such key.
int adapterSettingSet(const String &key, int value);
int adapterSettingFind(const String &key);
int adapterSettingValueAt(int i);
const char *adapterSettingKeyAt(int i);
