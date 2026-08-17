# LED-strip-lava

RGB LED strip controlled by M5Atom Lite for lava/fire effect. Used in the RS700 Demo installation with four modules in series, synchronized via TTL signals.

---

## Version status — this is the development branch

**You are on `Experiment`. The active development version is `RS700_Demo_Rev2_0/RS700_Demo_Rev2_0.ino`.**

| | Production v1 | Rev 2.0 (here) |
|---|---|---|
| Branch | `main` | `Experiment` |
| Sketch | `RS700_Demo_rev_6` | `RS700_Demo_Rev2_0/RS700_Demo_Rev2_0.ino` |
| Status | Frozen August 2026, deployed | Under development |
| Changes allowed | Documentation only | Yes |

Production version 1 was frozen in **August 2026** and is preserved unchanged on `main`, so a unit in the field can always be reflashed with exactly the code it shipped with. `RS700_Demo_rev_6` is also kept in this branch as the untouched baseline to diff Rev 2.0 against — **do not edit it here either**. All work goes into `RS700_Demo_Rev2_0/`.

Note that Rev 2.0 uses a proper Arduino sketch folder (`RS700_Demo_Rev2_0/RS700_Demo_Rev2_0.ino`) so it opens directly in the Arduino IDE, unlike the extensionless `RS700_Demo_rev_6`.

The previous contents of this branch (the pre-production sketches `Led_strip_lava_v3.ino`, `Led_strip_lava_v5.ino`, `RS700_Demo_1.ino`, `RS700_Demo_2.ino`) are preserved in the tag **`experiment-legacy-2026-04`**.

---

## Changes in Rev 2.0

| ID | Change | Status |
|---|---|---|
| R2.0-1 | Debounce on **both edges** of TTL IN. Originally only the falling edge was protected; a code review found the rising edge (pulse end) had no debounce at all, so a noise glitch shortly after a genuine falling edge could end the width measurement early. Fixed by requiring 50 ms of quiet since the last edge (either direction) before any edge is acted on | Untested (no TTL hardware exercised yet) |
| R2.0-2 | New **DEMO mode**: long press (≥ 1000 ms) from OFF/DONE ignites all modules instantly in purple (`#9255C0`, Interwell's lightest purple), skipping the sequential fill — reuses the existing Fire2012 flicker in its "full flame" state. No timeout; stopped the same way as PLAYING, with a long press (same 5 s fade-out) | Tested — see R2.0-5 |
| R2.0-3 | `NUM_PIXELS`/`USED_LEDS` raised from 263 to 272 (largest module seen so far), `getLedsForRing()` simplified to a flat 16/ring (272 = 17×16 exactly). One firmware image runs on modules with fewer physical LEDs — the NeoPixel chain simply doesn't deliver data past the last real LED, so a shorter module just has an incomplete outermost ring, nothing breaks | Tested OK on hardware |
| R2.0-4 | Burn-up timeline halved proportionally: `FILL_DURATION_SEC` 10→5, `FULL_FLAME_END_SEC` 16→8, `FADE_END_SEC` 30→15, `BLEND_DURATION` 6→3. `TTL_START_PULSE_SEC`'s offset also scaled (1.0s→0.5s) to keep the inter-module trigger at the same relative point in the flame | Tested OK on hardware |
| R2.0-5 | Fixed DEMO's purple render: `heatToColor()` is tuned for a fire palette (blue held near 0), which produced a dark/wrong color when given a purple base whose blue channel dominates. Added `heatToColorUniform()` — scales all three channels proportionally with heat — used only for DEMO | Fix applied, needs hardware re-test |

First hardware test (2026-08-17): button, normal ignition, LED count and halved timing all confirmed working. DEMO mode triggered but rendered the wrong color (see R2.0-5) — needs re-test.

---

## TODO

### Pending robustification measures

| Measure | Type | Risk | Interferes with code | Status |
|---|---|---|---|---|
| Debounce on both edges | SW | None | Minimal change | Done in Rev 2.0 (R2.0-1) |
| RC filter 100 Ω + 100 nF on TTL_IN_PIN | HW | None | No | Open |
| External 10 kΩ pullup to 3.3 V | HW | None | No | Open |

See [ideas.md](ideas.md) for hardware and effect ideas queued for this version.

---

## Setup, installation, board selection and libraries

### Arduino IDE 2.x – ESP32 support

1. Open **File → Preferences**.
2. In "Additional Boards Manager URLs" add:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Go to **Tools → Board → Boards Manager…**, search for "esp32" and install **ESP32 by Espressif Systems**.
4. Select board: **Tools → Board → ESP32 Arduino → M5Stack-ATOM**.
5. Select port: **Tools → Port → COMxx** (whichever port the Atom is on).
6. Set **Tools → Upload Speed → 115200** for reliable uploads.

### Required libraries

In **Sketch → Include Library → Manage Libraries…**:

- **Adafruit NeoPixel** – for the LED strip
- **M5Atom** – for the Atom Lite button and internal LED

---

## Physical wiring

**Board:** M5Stack Atom Lite (ESP32‑PICO‑D4)

**LED strip:** up to 272 addressable RGB LEDs arranged in 17 rings of 16 LEDs each (`NUM_PIXELS`/`USED_LEDS` = 272, sized for the largest module seen). Physical modules with fewer LEDs (e.g. 263) run the same firmware unmodified — the chain protocol means data addressed past the last real LED simply has no effect, so a shorter module just has an incomplete outermost ring.

| Signal | GPIO | Note |
|---|---|---|
| LED data | 22 | White JST wire |
| GND | GND | Black JST wire |
| 5V | 5V | Separate 5 V supply recommended at full brightness |
| TTL out | 19 | Active LOW, idle HIGH |
| TTL in | 21 | Active LOW, INPUT_PULLUP |

**Note:** A common GND between all modules is required for correct TTL communication.

---

## Ignite button

| Action | From mode | Result |
|---|---|---|
| Short press (< 1000 ms) | OFF / DONE | START sequence (normal red/orange burn-up) |
| Long press (≥ 1000 ms) | PLAYING | STOP — 5 s fade-out, then DONE |
| Long press (≥ 1000 ms) | OFF / DONE | **DEMO** — instant purple "full flame" on all modules, no timeout |
| Long press (≥ 1000 ms) | DEMO | Stop DEMO — same 5 s fade-out, then DONE |

Any press while `FADING` or while already in the target mode has no effect. Debounce between button events: 500 ms. Long-press actions fire immediately once the hold crosses 1000 ms (not on release).

---

## TTL protocol (module synchronization)

Four modules are connected in series. Module 1 is master and sends TTL pulses to downstream modules.

| Pulse width | Meaning |
|---|---|
| < 1000 ms LOW | START |
| ≥ 1000 ms LOW | STOP |

- Idle: HIGH
- Active: LOW
- Debounce: 50 ms (falling and rising edge)

---

## RS700 Cylinder Fire Effect

### Origin

The algorithm is based on **Fire2012** by Mark Kriegsman (FastLED project):
- Code: https://github.com/FastLED/FastLED/blob/master/examples/Fire2012/Fire2012.ino
- Article: https://blog.kriegsman.org/2014/04/04/fire2012-an-open-source-fire-simulation-for-arduino-and-leds/

The following is retained from Fire2012 conceptually:
- `heat[]` array along the vertical axis
- Three-step per-frame logic: **COOLING → HEAT DIFFUSION → SPARKING**
- `COOLING` and `SPARKING` parameters

The rest of the implementation is written from scratch using **Adafruit NeoPixel** instead of FastLED. `qsub8`, `qadd8`, and `random8` are the only FastLED helper functions retained directly.

### Geometry

The strip is treated as a cylinder with 17 rings. `FLAME_XY(ring, col)` maps (ring, column) to a linear LED index. No serpentine mapping — all rings are addressed linearly from `ringBase(ring)`.

### Animation sequence

| Phase | Time | Description |
|---|---|---|
| Fill | 0 – 5 s | Fire fills the cylinder ring by ring, bottom to top |
| Full flame | 5 – 8 s | Full fire effect, COOLING=40, SPARKING=120 |
| Color shift | 8 – 15 s | Color blends from dark red → orange via smoothstep |
| Blend to glow | 15 – 18 s | Fire algorithm fades out, lava flicker fades in |
| Lava glow | 18 s – | Steady lava glow with sine-based brightness pulsing |
| All off | 300 s | Sequence stops automatically |

**DEMO mode** (long press from OFF/DONE) skips this whole timeline: it locks into the "full flame" parameters permanently, recolored purple (`#9255C0`), with no color/glow progression and no auto-off — it stays until stopped with another long press.

### Modifications from Fire2012

- Ported from FastLED to Adafruit_NeoPixel
- 2D column-based `heat[FIRE_COLS][RINGS]` instead of 1D
- Per-column `colOffset[]` and `colRotation[]` for organic variation
- Custom `heatToColor()` with red/orange lava palette instead of FastLED's `HeatColors`
- `lavaFlicker()` — deterministic pseudorandom glow effect based on LED index and time
- `baseColorForTime()` — time-driven color palette shift via smoothstep interpolation
- `fadeScale` — global brightness ramp-down during FADING state
- TTL in/out with ISR and debounce for multi-module synchronization
- Button: short press = START, long press (≥ 1 s) = STOP
