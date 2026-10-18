# VBand ESP32

Standalone ESP32 client for [VBand](https://hamradio.solutions/vband/): keys over WiFi via a straight key, cycles channels with a pushbutton, shows connection status on an LED, and plays back other users' code as an audio sidetone.

Paired with a K3NG keyer (Arduino Mega, `w7yfr` branch with `FEATURE_VBAND_LINK`), it also becomes the keyer's VBand adapter: keying moves to VBand automatically while it's connected, the keyer's OLED shows WiFi/VBand status, who's in the room, and the conversation with each sender's tag, and the keyer can send it commands (see [Linking to the K3NG keyer](#linking-to-the-k3ng-keyer)).

## Pinout (Wemos D1 Mini32)

| Signal | GPIO | Notes |
|---|---|---|
| Keying line | 22 (D1) | Plain `INPUT`, active-high from the keyer's dedicated VBand line (Mega D7) through a 10k/20k divider -- see `pins.h` |
| Channel button | 17 | `INPUT_PULLUP`; same physical button that engages the power latch -- see power latch diagram below |
| Status LED | 15 | Active-high, through a current-limiting resistor to GND |
| Power latch (hold) | 4 | Output -- see power latch diagram below |
| Sidetone audio out | 25 | Built-in DAC1 output -- see wiring below |
| Keyer link TX | 18 (D5) | `Serial2` TX to Mega RX2 (D17) through a 10k series resistor -- see `pins.h` |
| Keyer link RX | 19 (D6) | `Serial2` RX from Mega TX2 (D16) through a 10k/20k divider (5V -> 3.3V) |

## Linking to the K3NG keyer

A two-way serial link (38400 baud) to the keyer's `Serial2`. Either board can be powered on or off at any time; the ESP32 always talks first, and the keyer keeps its TX pin high-impedance until it hears it.

**Wiring** (Mega is 5V, ESP32 is 3.3V, so the Mega -> ESP32 lines go through dividers):

```
  Mega D16 (TX2) ──[10k]──●── GPIO19 (link RX)        Mega D7 (VBand key line) ──[10k]──●── GPIO22 (key)
                          │                                                             │
                        [20k]                                                         [20k]
                          │                                                             │
                         GND                                                           GND

  GPIO18 (link TX) ──[10k]── Mega D17 (RX2)           Mega GND ── ESP32 GND
```

**What it does:**
- **Keying:** while the ESP32 reports VBand is ready (joined a channel), the keyer moves keying to its VBand line (TX 2, Mega D7) and back to the radio (TX 1) when VBand goes away. It switches only between characters and doesn't save the change.
- **Status screens** on the keyer's OLED: WiFi, the setup portal, VBand connecting/offline/lost, the channel joined and who's in it, OTA progress.
- **The conversation:** other users' sending is decoded on the ESP32 (a port of VBand's own adaptive decoder) as the sidetone plays it, and shown with a tag for each sender: their callsign if their VBand name has one, otherwise the start of their name. Each change of sender starts a new line; your own sending shows as `ME>`. On no-decode channels (`... (ND)`) only the tag is shown.
- **Joins and leaves:** "X joined" / "X left" lines (`vb.join`). The server doesn't push room changes, so the user list is polled every 30 s.
- **Your own tag:** your sending shows under the tag found in your VBand name (`vb.name`), the same way everyone else's is; `ME` until the adapter says.
- **The lobby and room counts:** connected to VBand but in no channel, so nothing you key goes anywhere (the keyer stays on its radio). On connecting, the keyer shows how many are in each room (`Lobby`, `Prac`tice, `Ch` 1-5, your custom room) for a few seconds before joining; `/ROOMS` shows them again, `/BUSY` joins the busiest channel.
- Status screens you asked for aren't wiped by incoming text; the conversation shows again when they time out.

**Keyer commands.** In the keyer's command mode, key `/` then a word and pause for a word space. The keyer checks the word with the ESP32 first: an unknown one shows "Unknown /XYZ" so you can try again; a known one runs, and its answer shows in command mode until it times out. Either way you stay in command mode: it only exits with the command button, `X`, or the menu's `< Exit` row.

| Command | Does |
|---|---|
| `/WHO` | Current channel and who's in it; off VBand, the menu offers to connect (key `R`) |
| `/CH` | Next channel (from the lobby or off VBand: back to your last one) |
| `/CH1`-`/CH5`, `/CHC` | Jump to a public channel, or `C` for your custom room |
| `/BUSY` | Join whichever channel (1-5 or your custom room) has the most people in it |
| `/ROOMS` | How many are in each room |
| `/LOBBY` | Leave the channel for the lobby |
| `/CON` / `/DIS` | Connect to VBand (into the lobby) / disconnect; the menu lists whichever applies, under Device |
| `/OTA` | Listen for an OTA update for 10 minutes (shows the IP); `vb.ota` |
| `/AP` | Restart into the WiFi / VBand settings portal |
| `/OFF` | Power off (on USB it can't, and says so) |
| `/DIAG` | Signal, uptime, disconnects, worst gap in received sending, dropped elements |
| `/LAST` | The latest resets and drops, newest first, kept across reboots (see Diagnostics) |
| `/H` | The list |

**Settings.** With `FEATURE_SETTINGS_MENU` on the keyer, the adapter's settings sit alongside the keyer's own, in groups: `ky` (keyer), `st` (sidetone) and `vb` (this adapter). They're saved on whichever board owns them (the adapter's in NVS), so they survive reboots and reflashing; the defaults are in `config.h`.

| Setting | Default | |
|---|---|---|
| `vb.send` | on | Not stored: whether your keying goes to VBand. Off keeps it here for practice while you stay in the room and still hear it; the LED stops following your keying and the keyer shows your practice without your name. Stays as set across rooms; on again at power-on. First in VBand > Commands, as "Sending" |
| `vb.name` | set in the portal | Your VBand name (spaces removed), what others see; your own tag comes from it |
| `vb.room` | set in the portal | Your custom room (spaces removed); rejoined if you're in it |
| `vb.start` | Last | On power-on: `Off` (connect with `/CON` or a channel), `Lobby`, `Last` channel, or your `Custom` room |
| `vb.ota` | On | Listen for OTA updates: `On`, `Off`, or `10min` then off (as `/OTA`) |
| `vb.rx` | Text | Others' sending on the display: `Text` with tags, `Sender` (tags only), or `Off` (audio only) |
| `vb.join` | on | "X joined" / "X left" lines |
| `vb.vol` | 100% | Received audio level, in 5% steps (the 8-bit DAC gets grainy turned well down; the P3 pot is smoother) |
| `vb.led` | on | The LED follows your keying while joined (VBand > Settings > LED > "Keying") |
| `vb.ann` | on | The LED flashes the channel's code in Morse after each join (LED > "Channel") |
| `vb.otaled` | on | The LED during an OTA update: a flicker while it arrives, a flash when it's done, a long flash if it fails (LED > "OTA") |
| `vb.status` | on | The LED's status blinks: slow while WiFi is down, faster while connecting or joining (LED > "Status") |
| `vb.chan` | | Not stored: where you are, `L` (the lobby), `1`-`5` or `C` (your custom room); choosing one goes there, connecting first if need be. In the menu under VBand > Commands > Rooms, as "Channel" |
| `vb.pitch` | on | A pitch per sender (off: everyone at `vb.tone`) |
| `vb.tone` | 700 Hz | The first sender's pitch, 300-1200 Hz |
| `vb.fade` | 5 ms | Sidetone fade in/out, 0-20 ms |
| `vb.win` | 10 s | Channel button: how soon a second press switches channel, 3-30 s |
| `vb.log` | off | Forward this log to the keyer's USB port (see Diagnostics). Not saved: off at every power-on |
| `vb.tls` | on | Connect to VBand on its encrypted port (7386, as the website does) rather than the plaintext one (7385), which has stalled and dropped connections. Takes effect at the next power-on. In the menu under Device, as "Encrypted" |
| `vb.phy` | b/g/n | Which WiFi generations the adapter may use: `b/g/n` (all), `b/g` (no 802.11n) or `b` (802.11b only). Older ones are slower but get through a busy 2.4 GHz channel more often, and VBand needs very little speed. If the router won't take the chosen mode, it falls back to b/g/n after 15 s. Takes effect at the next power-on. In the menu under Device, as "WiFi mode" |
| `vb.ui` | Off | The VBand transcript on the keyer's USB port (see Transcript): `Off`, `On` (until power-off) or `Always` (saved). In the menu under Display, as "Transcript" |

Three ways to reach them:
- **Menu:** in command mode, key `/` and pause. Dit (`E`) moves down, dah (`T`) up, `R` opens a group, toggles an on/off setting, runs a command, or starts and saves a change (meanwhile dah raises a number and dit lowers it; in a list of options, dit goes to the next one and dah back, the way rows go), and `B` or `< Back` go back a level (`B` on the top level closes the menu). `X`, the command button, or `< Exit` leave command mode. The VBand group first offers Commands (the ones below) or Settings; each opens the lists that hold them: Commands has Rooms (Who's here first, then a Channel selector) and Device (Connect or Disconnect first); Settings has Rooms, Sound, Display, LED and Device. Running a command shows its answer, then the menu comes back where you were (a paddle touch skips the wait).
- **Keyed shortcuts:** `/VB LED OFF`, `/KY WPM 22`, `/VB NAME W7YFR`; leave off the value to see the current one, or key just the group (`/VB`) and pause to open its menu.
- **The keyer's CLI:** `\$` lists everything, `\$ vb` a group, `\$ vb.fade` one setting, `\$ vb.fade 8` sets it (`\$ vb.name Rob W7YFR` keeps your capitals and drops the spaces).

The keyer only offers the settings its build supports (Farnsworth with `FEATURE_FARNSWORTH`, autospace with `FEATURE_AUTOSPACE`, the transmitter with more than one, PTT times with a PTT line, and so on).

**Protocol.** Frames are `$TYPE,fields*XX\n`, `XX` being the XOR of the bytes between `$` and `*` in hex; anything malformed is dropped. Either side's link is "up" while frames keep arriving (6 s timeout).

| Direction | Frame | Meaning |
|---|---|---|
| ESP -> keyer | `MY,<tag>` | Your own tag, from your VBand name; on link-up and when the name changes |
| both | `HI,<protocol>` | Heartbeat: ESP32 every 1 s until answered, then 2 s. Each side warns on the OLED ("Link Mismatch / Update keyer" or "Update adapter") if the other's protocol version differs |
| ESP -> keyer | `VB,1` / `VB,0` | VBand usable or not; sent on change and with every heartbeat |
| ESP -> keyer | `ST,<ms>,<row>\|<row>...` | Status screen, up to 4 rows |
| ESP -> keyer | `OF,<command>,<label>` | Goes with a command's answer: the keyer's menu offers the command ("R: <label>") and runs it if you key R while the answer shows. `/WHO` off VBand offers `CON` |
| ESP -> keyer | `STU,<ms>,<row>\|<row>...` | An update to the status screen, shown only while it's still up (nothing else has replaced it). The connecting screen sends one every second with the attempt and the time so far, so it stays up until VBand joins, connecting stops, or you key |
| ESP -> keyer | `RX,<tag>,<text>` | Decoded text from another station |
| ESP -> keyer | `SYS,<text>` | A line of its own, e.g. "X joined" |
| ESP -> keyer | `LG,<text>` / `LGP,<text>` | A line of this log, or part of a longer one (more follows), with `vb.log` on; the keyer prints it on its USB port |
| ESP -> keyer | `TR,<text>` / `TRP,<text>` | The transcript, with `vb.ui` on: text that ends the current line, or that goes on it (more follows); the keyer prints it on its USB port |
| ESP -> keyer | `UI,1` / `UI,0` | The transcript is streaming your sending or not (it's on, and `vb.send` is on); on change and every 2 s. While it is, and VBand is on, the keyer doesn't echo your sending to its USB port, since the transcript has it |
| ESP -> keyer | `SN,1` / `SN,0` | Your keying goes to VBand or stays here (`vb.send`); on change and every 2 s. The keyer tags your sending with your name only while it goes to VBand |
| ESP -> keyer | `BYE,<reason>` | About to reboot or power off (`OTA`, `OFF`, `AP`) |
| keyer -> ESP | `CK,<word>` | Is this a command? |
| ESP -> keyer | `CR,1` / `CR,0` | Known / unknown |
| keyer -> ESP | `CMD,<word>` | Run it (sent after leaving command mode) |
| keyer -> ESP | `SL` / `SL,s` / `SL,a` | List the settings and commands / just the settings / just the commands |
| ESP -> keyer | `SI,<i>,<group>,<key>,<label>,<type>[,<value>,<min>,<max>,<step>,<unit>]` / `SE,<count>` | One per item (type `b` on/off, `n` number, `e` one of the options named in `<unit>`, separated by bars, `s` text, `a` command), then the end. `<group>` is the keyer menu list it goes in under VBand > Settings or Commands: `R` Rooms, `A` Sound, `D` Display, `L` LED, `H` Device, `-` none (since protocol 2) |
| keyer -> ESP | `SG,<key>` / `SS,<key>,<value>` | Read / set a setting |
| ESP -> keyer | `SV,<key>,<value>` | Its value (`?` if there's no such setting) |

## Connection health

The adapter connects on VBand's encrypted port by default (`vb.tls`). A connection the server stops answering is
dropped and made again rather than left hanging: no answer to the login within 10 s, or, in a channel, no answer to
the user list it asks for every 30 s within 15 s. Each such reconnect is logged and noted in the transcript
(`* VBand didn't answer ...; reconnecting`).

## Transcript

The adapter keeps the VBand session as a transcript: everyone's sending (yours too, decoded from the key line), joins
and leaves, room changes and connects and drops, each line starting with the UTC time from NTP (`[--:--Z]` until it
has answered):

```
[14:02Z] * Joined Channel 2
[14:02Z] * 2 here: K1ABC (Bob K1ABC), N0XYZ (Ann N0XYZ)
[14:02Z] W7YFR: CQ CQ DE W7Y
[14:02Z] K1ABC: QRL?
[14:02Z] W7YFR: CQ CQ DE W7YFR W7YFR K
[14:03Z] * N0XYZ left
```

A message is one person's sending until they've been quiet for 5 s, as on the keyer's display. On no-decode channels
a line shows who's sending, without the text. The transcript doesn't depend on `vb.rx` or `vb.join`.

- **Live:** with `vb.ui` on, the keyer prints it on its USB port as it's decoded (in CLI mode only, as the log), and
  stops echoing your sending there, which the transcript carries. When someone who was interrupted goes on, their
  new line repeats their whole message so far, so each line reads whole (as above). If the link can't keep up, a
  line says some was lost.
- **Recorded:** always, whatever `vb.ui` says: each finished message as one line, in the order the messages started,
  without the repeats; the last 32 KB (about 300 messages), in RAM, so lost at power-off. `/DIAG` logs how much it
  holds. Exporting it is still to come.

## Versions

While this is being iterated on, build the adapter from the latest `main` and the keyer from the latest `w7yfr`. What has to match between them is the **link protocol** version: both sides send it in `HI` and warn on the keyer's display ("Link Mismatch / Update keyer" or "Update adapter") if they differ, so flashing only one board after a breaking change says so. It only changes when a message format changes in a way the other side can't handle -- new message types don't need it, since both sides ignore types they don't know. Bump `MEGA_LINK_PROTOCOL_VERSION` here and `VBAND_LINK_PROTOCOL_VERSION` in the keyer together.

The adapter shows the commit it was built from (`+` if there were uncommitted changes) with the WiFi status at boot and in `/DIAG`, so after an OTA update you can see what's running.

## Channel button

- **Press:** shows the current channel and who's in it on the keyer (in the lobby, the room counts).
- **Press again within 10 s** (the `vb.win` setting) of the last press: next channel (Channel 1-4, Channel 5 (ND), your custom room, around again); off VBand or in the lobby, it connects and rejoins your last channel. Presses act on release.
- **Hold 2 s:** power off (the LED flashes until you let go). On USB power it can't cut its own power, so after a moment it carries on and the keyer shows "Still Powered".
- **Hold through power-on:** opens the WiFi / VBand settings portal (or key `/AP`).

The last channel joined is remembered and rejoined after a reboot.

## Status LED

- Blinking slowly: WiFi not connected. Blinking faster: connecting to VBand or joining a channel (the `vb.status` setting).
- Off in the lobby, or when off VBand on purpose.
- After each join, flashes the channel number (or `C`) in Morse (the `vb.ann` setting).
- While joined, lights with your keying (the `vb.led` setting).
- If the connection drops, the LED starts blinking right away, but what's already been received keeps playing (and
  showing on the keyer) at the sender's speed. After a long, fast message that can run on for a while. That's on
  purpose: it's real sending, just behind. Nothing sent during the drop arrives later.

## Soft power latch

There's a single physical pushbutton in this build, and it does triple duty. A P-channel MOSFET switches the board's supply; pressing the button pulls its gate low, which is what first powers the board on. `PIN_POWER_LATCH` then takes over holding the gate low once firmware is running (see `power_latch.cpp`), so the button can be released. That same button is also `PIN_CHANNEL_BUTTON` -- wired through a protection diode so the circuit's ~5V gate node can't reach that 3.3V-only pin -- which is how it cycles channels in software (`channel_button.cpp`).

**Parts:** M1 = IRF5305 (P-channel MOSFET), Q1 = 2N2222A (NPN BJT), D1/D2 = 1N4148, SW1 = the single momentary pushbutton. R1 = 1MΩ, R2 = 10kΩ, R3 = 1MΩ.

```
5V raw ───────●─────────────────────────────┐
              │                             │ source
           [R1 1M]                        ┌─┴─┐
              │                     gate  │M1 │  IRF5305 (P-ch MOSFET)
              ●─────────────●─────────────┤   │
              │             │             └─┬─┘
              │ collector   │               │ drain
            ┌─┴─┐           ▼  D1           └────►  ESP32 5V pin (switched)
   ┌────────┤Q1 │          ─┬─
   │   base └─┬─┘           │
   │          │ emitter     │
   │         GND            │
   │                        │
   │                     SW ●──────────┐
   │                        │          │
   │                      [SW1]       ─┴─
   │                    pushbutton     ▲  D2
   │                        │          │
   │                       GND      GPIO17  PIN_CHANNEL_BUTTON (INPUT_PULLUP)
   │
   ●───[R2 10k]─── GPIO4  PIN_POWER_LATCH
   │
[R3 1M]
   │
  GND
```

`●` marks every point where three wires join; plain corners have no dot. Each diode is drawn as a triangle pointing at a bar -- the bar is the striped (cathode) end. Both D1's and D2's stripes face the `SW` node: D1's anode is on M1's gate, D2's anode is on GPIO17.

Driving `PIN_POWER_LATCH` HIGH turns on Q1, which pulls M1's gate to near GND, turning M1 on and powering the board -- that's the hold. Pressing SW1 does the same thing to the gate through D1, which is what gets the board powered up in the first place, before firmware is even running; R1 is what keeps M1 off (gate pulled to 5V) when neither the button nor Q1 is active. D2 lets `PIN_CHANNEL_BUTTON` read that same press directly (LOW while held) without ever seeing more than a diode drop above GND -- the gate and `SW` can sit near the raw 5V rail, well above what a 3.3V-only ESP32 pin can tolerate.

Check your actual parts' datasheets for lead order (E/B/C, G/D/S) -- this gives logical connections, not physical pinout.

## Feeding the sidetone audio into your amp

`PIN_AUDIO_OUT` (GPIO25) drives one of the ESP32's built-in 8-bit DACs, synthesizing a sine-wave tone for code received from other users (see `sidetone.cpp`).

- **A pitch per sender** (the `vb.pitch` setting; pitches in `AUDIO_SENDER_TONES_HZ`) so several people in a room can be told apart: the first sender heard gets 700 Hz, the next 550, 850, 600, 800 Hz and so on.
- **Fade in and out** over 5 ms (the `vb.fade` setting, a raised-cosine envelope) instead of switching at full level, which clicks.
- **Timing:** elements are replayed at the sender's own timing, never faster. If the network delivers a burst after a stall, playback runs behind until the sender's next pause, then catches up (silence that has already gone by isn't replayed). Up to 128 elements are held (`AUDIO_QUEUE_CAPACITY`).
- The sample interrupt runs at 20 kHz, only while a tone is playing or fading out. It's meant to be summed into the **same amplifier/speaker circuit your keyer's own sidetone already feeds** -- not wired into a port on the keyer itself.

This has been wired against a specific build: a K3NG keyer (Arduino Mega) whose sidetone already runs `D52 (VOL POT) -> pot wiper -> C4 (3.3uF) -> R3 (4.7k) -> LM386 IN`. Adjust component references below if your build differs, but the approach generalizes: splice a second AC-coupled source into whatever node already feeds your amp's input.

**Splice point:** the junction where C4 meets R3. D52's active drive is already blocked by C4 before reaching that point, so it's a purely passive AC node from the Mega's side.

**Why the ESP32 branch needs its own resistor, not just P3.** GPIO25 is never a quiet, high-impedance pin -- between tones the DAC holds it at a fixed mid-scale level, and during them it drives the tone. It's always a stiff, low-impedance point, the same as D52 is. R3 protects the Mega's own signal from D52's stiff output, but R3 is now shared, so it doesn't protect the Mega's signal *from the ESP32 branch*. Without a resistor of its own, the only thing standing between the shared node and GPIO25 is P3's own resistance -- which goes to ~0 ohms at full volume. At that setting, `shared node -> C8 -> P3 -> GPIO25` becomes a low-impedance path that siphons the Mega's own signal off to GPIO25's fixed output instead of letting it continue to R3/LM386, silencing it. **R8** fixes this: a fixed resistor between C8 and the shared node, sized meaningfully larger than R3 (start around 22k) so it doesn't out-compete the Mega's path into R3 even at P3's lowest-resistance setting. The trade-off is the same resistor also attenuates the ESP32's own tone somewhat -- normal for a passive mixer, and tunable: raise R8 if the Mega's sidetone is still being pulled down at high P3 settings, lower it if the ESP32's own tone is too quiet.

**New parts** (continuing the existing C1-C6/R1-R7 numbering):
- **P3** -- a second volume knob just for the VBand tone: a 10k linear trimmer or panel pot, wired the same way as P2 (GPIO25 drives the wiper directly, one outer leg to GND, other outer leg continuing on). It's not tied to the same physical knob as the keyer's own sidetone (P2). 
- **C8** -- 1uF coupling cap, positioned after P3 (mirroring where C4 sits after P2). The DAC idles at **~1.65V**, not 0V, so this blocks that DC bias from reaching the shared node -- the same job C4 already does for the Mega's branch. Ceramic/film sidesteps polarity questions; if electrolytic, `+` toward P3's output leg (the side closer to GPIO25's ~1.65V bias) -- matching how C4 is oriented relative to P2/D52.
- **R8** -- fixed resistor after C8, start at 22k (see explanation above for why this needs to be meaningfully larger than R3, not similar to it).

```
  Mega D52 (VOL POT) ---o W [P2 500K]--| C4 3.3uF |-----+
                          |  (existing)  (existing)     |
                          GND                           |
                        (other leg)                     +--[ R3 4.7k ]--o LM386 IN
                                                        |
  ESP32 GPIO25 ---------o W [P3 10k]-| C8 1uF |--[ R8 ]-+
  (PIN_AUDIO_OUT)         |    (NEW)     (NEW)   22k NEW
                          GND                  (start here, tune to taste)
                        (other leg)

  ESP32 GND ------------------------------- shared ground rail
```

Turning P3 down shifts more of the AC signal to the grounded leg instead of on toward C8/R8, reducing both the ESP32's own tone volume and (as a side effect, now that R8 is in place) any residual loading on the Mega's signal even further.

## Building & flashing

The build scripts are a git submodule (`scripts/`), so clone with `git clone --recursive`, or run `git submodule update --init` after a plain clone.

```bash
pio run -e wemos_d1_mini32 -t upload       # USB
pio run -e wemos_d1_mini32_ota -t upload   # OTA -- see scripts/README.md for .env setup
```

The `VBand-ESP32` setup hotspot's password defaults to `w7yfr-vband`; override it with `WIFI_MANAGER_AP_PASSWORD` in `.env` (8+ characters).

OTA listens per the `vb.ota` setting: `On` (the default, `OTA_MODE_DEFAULT` in `config.h`), `Off`, or `10min` (`/OTA` does the same), after which it's off again. A window never cuts off an update in progress.

## Diagnostics

With `DIAGNOSTICS_LOG` defined (the default), the USB serial port gets a `DIAG` line every 5 s -- WiFi signal, free heap, the slowest pass through each part of `loop()`, how other users' keying is arriving (count, longest gap between elements and between any server messages), how far playback is behind, dropped elements, disconnects, and how our own elements go out (`tx` sent, `txlate` the latest one handed to the network after the key came up, `txfail` sends that failed) -- plus a line for each WiFi or VBand drop with its reason. Nothing reads it without a serial monitor, so it costs next to nothing otherwise. To capture a log:

```bash
pio device monitor -e wemos_d1_mini32 | tee diag-log.txt   # -e: the default env is OTA
```

`/DIAG` on the keyer shows a summary on its display.

**Why it went away.** The adapter keeps its last 8 resets and drops in NVS, so they survive a reboot or a power
cycle: resets other than power-on and its own restarts (OTA, the portal), with the reason (`brownout`, `crash`,
`watchdog`, ...); WiFi drops, with the reason code and signal; and VBand drops it didn't ask for. Failed reconnect
attempts aren't counted again, and the same thing repeated in one boot shows once with a count. `/LAST` shows the
latest three on the keyer: `Brownout -2b` (two boots ago), `WiFi r200 12m` (12 minutes ago, reason code 200),
`Reset: crash` (what started this boot). `/LAST` and `/DIAG` also log the whole list, with the reasons' names, and
it's logged at every boot after a `DIAG boot <n>, reset: <reason>` line.

**Without opening the case.** Only the keyer's USB port can be reached with the adapter in its case. Turn on
`vb.log` (`\$ vb.log on` in the keyer's CLI, or the menu) and this log also comes out of the keyer's port, each line
starting `ESP> `; it starts with the last 32 lines, so you see what happened before you plugged in. A log line goes
over the link only when nothing else is waiting, at most one frame every 40 ms; if they can't keep up, they're dropped
with a "(N lines dropped)" note. The per-element `TX space=` / `RX ... space=` lines stay on the adapter's own port.
`vb.log` turns itself off at power-on. On the keyer, with the USB port in Winkey mode nothing is printed.

```bash
pio device monitor -e w7yfr | tee diag-log.txt   # in the keyer's repo
```

## Notes for future changes

Lessons from building this, so they aren't relearned:
- **Don't call `dacWrite()` from an interrupt.** In this core (2.0.17) it redoes the DAC pad setup on every call; at audio rates that starved `loop()` and looked like random hangs. `sidetone.cpp` writes the DAC register directly from IRAM.
- **Don't trust disabling the timer alarm to stop a tone.** The timer driver re-arms an auto-reload alarm after each interrupt, so a stop can lose the race and leave the tone running. The interrupt checks its own on/off flag.
- **Time the key line in an interrupt.** Polling it from `loop()` lost whole elements while `ws.sendTXT()` waited on the network.
- **Keep WiFi modem sleep off.** It delayed and lost received packets, and the losses stalled the VBand connection for seconds while TCP retransmitted.
