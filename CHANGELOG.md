# Changelog

VBand ESP32 adapter (Wemos D1 Mini32). Part of the [Stellaluna Keyer](https://github.com/W7YFR/stellaluna); the
keyer side is [W7YFR/k3ng_cw_keyer](https://github.com/W7YFR/k3ng_cw_keyer).

Builds show their git commit at boot and in `/DIAG`. The keyer link protocol is version 2.

## Unreleased

### Added
- `vb.tls` (Device > Encrypted, on by default): connect on VBand's encrypted port (7386), as the website does. On the
  plaintext port (7385) connections kept stalling: logins went unanswered for minutes, sends to the server blocked for
  seconds, and the connection dropped every minute or two. Takes effect at the next power-on.
- A connection the server stops answering is dropped and made again: no answer to the login within 10 s, or, in a
  channel, none to the user list within 15 s. The login's send and library errors are logged.
- The DIAG line shows how our own elements go out: how many, the latest one handed to the network after the key came
  up (`txlate`), and failed sends (`txfail`).
- The VBand transcript: everyone's sending (yours too, decoded from the key line), joins and leaves with the full
  name, the room's list on joining, room changes, connects and drops, each line starting with the UTC time from NTP
  (`[14:02Z] W7YFR: CQ CQ`). `vb.ui` (Off / On until power-off / Always, saved) streams it to the keyer's USB port as
  it's decoded (`TR` / `TRP` frames; `UI` says whether it's on, so the keyer stops echoing your sending there). An
  interrupted sender's next line repeats their whole message so far. It's also always recorded, one line per finished
  message in the order they started, the last 32 KB in RAM; `/DIAG` logs how much. See the README's Transcript.
- Log forwarding (`vb.log`, off at every power-on, never saved): the adapter's log also goes over the link to the
  keyer, which prints it on its USB port, the only one reachable with the adapter in its case. It starts with the
  last 32 lines, so you see what happened before you plugged in. Log frames (`LG` / `LGP`) only go out while nothing
  else is waiting on the link; if they can't keep up, lines are dropped with a note. The per-element `TX` / `RX`
  lines stay on the adapter's own USB port.
- Why it went away: the last 8 resets (brownout, crash, watchdog, ...; not power-on or its own restarts), WiFi
  drops (reason code and signal) and unexpected VBand drops (how long the server had been silent, and signal) are
  kept in NVS across reboots and power cycles. `/LAST` shows the latest three on the keyer; `/LAST` and `/DIAG` log
  the whole list, and it's logged at every boot with the reset reason. Failed reconnect attempts, `/DIS` and going
  to the lobby aren't recorded as drops.

- The connecting screen stays up: "Connecting...", "Retrying..." and "VBand Lost / Reconnecting" show the attempt
  and a clock, refreshed every second (`STU` frames), until VBand joins, connecting stops, or you key over it.
- LED settings, in their own menu list (VBand > Settings > LED): Keying (`vb.led`), Channel (`vb.ann`, the channel's
  code flashed on joining), OTA (`vb.otaled`, the update's flicker and flashes) and Status (`vb.status`, the blinks
  while WiFi is down or VBand is joining).
- A Channel selector (`vb.chan`, not stored) in VBand > Commands > Rooms: where you are (L for the lobby, 1-5, C), and
  choosing one goes there, connecting first if need be. Commands now opens Rooms (Who's here first) and Device
  (Connect or Disconnect first).
- Who's here off VBand offers Connect (`OF,CON,Connect`): key R in the keyer's menu to connect.
- Menu groups (link protocol 2: flash the keyer too): each setting and command the keyer lists says which menu
  list it goes in (`SI,<i>,<group>,...`), so the keyer shows VBand > Commands and Settings each with their
  most-used rows on top and the rest under Rooms, Sound, Display and Device. Tone now comes before Pitches.

### Fixed
- Who's here while connecting no longer counts as another attempt on the connecting screen.
- "Room X: N others" is logged when the count or who's there changes: once per join instead of twice, and no longer
  every 30 s. Playback after a drop is unchanged on purpose (what was received keeps playing); the README says so.

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
