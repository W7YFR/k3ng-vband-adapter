# Changelog

VBand ESP32 adapter (Wemos D1 Mini32). Part of the [Stellaluna Keyer](https://github.com/W7YFR/stellaluna); the
keyer side is [W7YFR/k3ng_cw_keyer](https://github.com/W7YFR/k3ng_cw_keyer).

Builds show their git commit at boot and in `/DIAG`. The keyer link protocol is version 1.

## Unreleased

## 2026-10-05: settings

### Added
- Settings the keyer can list, read and change over the link (`SL` / `SG` / `SS`), shown in the keyer's menu, with
  the commands as menu actions.
- Startup mode: Off, Lobby, Last channel or a custom room.
- Lobby (connected without joining a channel), room counts, and joining the busiest room (`/LOBBY`, `/ROOMS`,
  `/BUSY`).
- OTA mode: Off, On, or a 10-minute window.
- Received text on the keyer: text, sender only, or off. Join / leave lines on or off.
- Received audio volume, keying LED on/off, a pitch per sender on/off, tone pitch, fade length, and the channel
  button's second-press window.
- VBand name and custom room (spaces stripped); a name change renames you without reconnecting.
- Connect / Disconnect (`/CON`, `/DIS`); reconnects quietly.
- Your own VBand tag sent to the keyer, so your sending shows under it.

## 2026-10-04: keyer link features

### Added
- Status screens on the keyer's display: WiFi, portal, VBand connecting / offline / lost, the channel and who's in
  it, OTA progress.
- Who's in the room, and who joins or leaves (the user list is polled every 30 s).
- Other users' sending decoded (a port of VBand's own decoder) and shown on the keyer with a tag per sender.
- A different sidetone pitch for each sender, and a fade in and out on the sidetone.
- Keyer commands (`/WHO`, `/CH`, `/OTA`, `/AP`, `/OFF`, `/DIAG`, `/H`), the channel button showing the room first and
  switching on a second press, and the keying LED.
- Rejoins the last channel after a reboot.
- Checks the keyer's link protocol version; shows the build's git commit.
- `DIAGNOSTICS_LOG` as a standing debug switch.

### Fixed
- The sidetone timer interrupt starved `loop()` (it now writes the DAC register directly).
- Stuck sidetone, playback lag, and stalls under heavy traffic (WiFi sleep off, playback timed from the sender, no
  replay of silence that already passed).
- The key line is timed in an interrupt; VBand connection trouble is shown.
- Channels switch on button release, not during a power-off hold.

### Changed
- Holds 128 received elements for playback.

### Docs
- README documents the keyer link, commands, button, LED and sidetone; the scripts submodule needs fetching on clone.

## 2026-10-03: K3NG keyer link and power latch

### Added
- Serial link to the K3NG keyer (Serial2 on GPIO18/19) and an active-high key line on GPIO22 from the keyer's D7.
- Tells the keyer when VBand is ready (`VB,1` / `VB,0`).
- Circuit test mode that exercises the key line and the keyer link, with the link state on the LED.
- Soft power latch: long press to shut down; power held through an OTA restart.
- Received-audio sidetone on the DAC.
- WiFi setup portal with a password, VBand settings in the portal, and re-entry; LED patterns at boot, for WiFi and
  for the portal; more OTA statuses.

## 2026-08-31: first version

### Added
- VBand client keying from a straight key, a channel button, a status LED, and OTA updates.
