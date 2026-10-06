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

enum class AdapterSetting { Start, Ota, Rx, Join, Volume, Led, Pitch, Tone, Fade, Window, Count };

// VB.START: what to do at power-on.
#define VBAND_START_OFF 0    // stay off VBand until /CON (or a channel is chosen)
#define VBAND_START_LOBBY 1  // connect and wait in the lobby
#define VBAND_START_LAST 2   // rejoin the last channel
#define VBAND_START_CUSTOM 3 // join the custom room

// VB.OTA: listen for updates.
#define OTA_MODE_OFF 0
#define OTA_MODE_ON 1
#define OTA_MODE_WINDOW 2 // for OTA_WINDOW_MS, then off; never saved

// VB.RX: what to show of other people's sending.
#define RX_SHOW_TEXT 0   // decoded text with each sender's tag
#define RX_SHOW_SENDER 1 // just who's sending (as on no-decode channels)
#define RX_SHOW_NONE 2   // nothing; audio only

void adapterSettingsBegin();

// A number setting's current value.
int adapterSetting(AdapterSetting setting);

// For the link (keyer_commands.cpp), by index over all settings, text
// ones (name, room) first.
int adapterSettingsCount();
// "<key>,<label>,<type>,<value>[,<min>,<max>,<step>,<unit>]": type b
// (on/off, value 0/1), n (number), e (one of a list: value is its index,
// unit is the options' names separated by |) or s (text, no range).
String adapterSettingDescription(int i);
// Index of a setting by key (case-insensitive), or -1.
int adapterSettingFind(const String &key);
// Sets a setting by key from its text form (numbers clamped to their
// range; an option by index or name; spaces and commas stripped from
// text) and saves it. Returns the index, or -1 if there's no such key or
// an empty text value.
int adapterSettingSet(const String &key, const String &value);
String adapterSettingValueText(int i);

// The 10-minute OTA window closed by itself: VB.OTA reads Off again.
void adapterSettingOtaWindowEnded();
