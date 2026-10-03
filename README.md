# VBand ESP32

Standalone ESP32 client for [VBand](https://hamradio.solutions/vband/): keys over WiFi via a straight key, cycles channels with a pushbutton, shows connection status on an LED, and plays back other users' code as an audio sidetone.

## Pinout (Wemos D1 Mini32)

| Signal | GPIO | Notes |
|---|---|---|
| Keying line | 16 | Plain `INPUT`, externally biased through a voltage divider -- see `pins.h` |
| Channel button | 17 | `INPUT_PULLUP`, momentary switch to GND |
| Status LED | 15 | Active-high, through a current-limiting resistor to GND |
| Sidetone audio out | 25 | Built-in DAC1 output -- see wiring below |

## Feeding the sidetone audio into your amp

`PIN_AUDIO_OUT` (GPIO25) drives one of the ESP32's built-in 8-bit DACs, synthesizing a 700 Hz tone for code received from other users (see `sidetone.cpp`). It's meant to be summed into the **same amplifier/speaker circuit your keyer's own sidetone already feeds** -- not wired into a port on the keyer itself.

This has been wired against a specific build: a K3NG keyer (Arduino Mega) whose sidetone already runs `D52 (VOL POT) -> pot wiper -> C4 (3.3uF) -> R3 (4.7k) -> LM386 IN`. Adjust component references below if your build differs, but the approach generalizes: splice a second AC-coupled source into whatever node already feeds your amp's input.

**Splice point:** the junction where C4 meets R3. D52's active drive is already blocked by C4 before reaching that point, so it's a purely passive AC node from the Mega's side.

**Why the ESP32 branch needs its own resistor, not just P3.** GPIO25 is not a quiet, high-impedance pin when it's not playing a tone -- `sidetone.cpp`'s timer interrupt rewrites the DAC output continuously, 40,000 times a second, whether or not a tone is sounding. It's always a stiff, low-impedance point, the same as D52 is. R3 protects the Mega's own signal from D52's stiff output, but R3 is now shared, so it doesn't protect the Mega's signal *from the ESP32 branch*. Without a resistor of its own, the only thing standing between the shared node and GPIO25 is P3's own resistance -- which goes to ~0 ohms at full volume. At that setting, `shared node -> C8 -> P3 -> GPIO25` becomes a low-impedance path that siphons the Mega's own signal off to GPIO25's fixed output instead of letting it continue to R3/LM386, silencing it. **R8** fixes this: a fixed resistor between C8 and the shared node, sized meaningfully larger than R3 (start around 22k) so it doesn't out-compete the Mega's path into R3 even at P3's lowest-resistance setting. The trade-off is the same resistor also attenuates the ESP32's own tone somewhat -- normal for a passive mixer, and tunable: raise R8 if the Mega's sidetone is still being pulled down at high P3 settings, lower it if the ESP32's own tone is too quiet.

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

```bash
pio run -e wemos_d1_mini32 -t upload       # USB
pio run -e wemos_d1_mini32_ota -t upload   # OTA -- see scripts/README.md for .env setup
```
