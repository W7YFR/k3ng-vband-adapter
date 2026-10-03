#pragma once

#include <Arduino.h>

// User-configurable VBand identity, persisted in NVS so it survives
// reboots and reflashing. Set from the WiFi config portal (see
// wifi_setup.cpp); read by vband_client.cpp when connecting.

// Loads saved values. On first boot, generates a default name of
// VBAND_DEFAULT_NAME_PREFIX plus a random number and saves it right away,
// so the same name is kept from then on. Call before wifiConnect().
void vbandSettingsBegin();

// Name shown to other users.
const String &vbandSettingsName();

// Custom room, the last stop in the channel button's cycle.
const String &vbandSettingsRoom();

// Saves new values. Commas (the VBand protocol's field separator) are
// stripped and whitespace trimmed; a value that ends up empty leaves the
// current setting unchanged.
void vbandSettingsSave(const String &name, const String &room);
