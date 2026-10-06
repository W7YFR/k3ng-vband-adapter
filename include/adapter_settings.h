#pragma once

#include <Arduino.h>

// Settings the keyer can list, read and change over the link -- from its
// command-mode menu, keyed shortcuts ("/VB LED OFF") and its CLI ("\$
// vb.led off"). Saved in NVS, so they survive reboots and reflashing;
// the defaults are in config.h. The VBand name and custom room are the
// same ones the setup portal sets (vband_settings.h).
//
// The list the keyer gets also carries the commands (keyer_commands.cpp)
// as actions, so its menu shows everything in one place.

enum class AdapterSetting { Led, Pitch, Tone, Fade, Window, Count };

void adapterSettingsBegin();

// A number setting's current value.
int adapterSetting(AdapterSetting setting);

// For the link (keyer_commands.cpp), by index over all settings, text
// ones (name, room) first.
int adapterSettingsCount();
// "<key>,<label>,<type>,<value>[,<min>,<max>,<step>,<unit>]": type b
// (on/off, value 0/1), n (number) or s (text, no range).
String adapterSettingDescription(int i);
// Index of a setting by key (case-insensitive), or -1.
int adapterSettingFind(const String &key);
// Sets a setting by key from its text form (numbers clamped to their
// range; spaces and commas stripped from text) and saves it. Returns the
// index, or -1 if there's no such key or an empty text value.
int adapterSettingSet(const String &key, const String &value);
String adapterSettingValueText(int i);
