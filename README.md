# LED-strip-lava

RGB LED strip controlled by M5Atom Lite for lava/fire effect. Used in the RS700 Demo installation with four modules in series, synchronized via TTL signals.

---

## Version status

**`main` now ships `RS700_Demo/RS700_Demo.ino` as the active production version**, tested and confirmed working on hardware — Rev 2.1, tagged `v2.1` (see "Changes in Rev 2.0" and "Changes in Rev 2.1" below). `Experiment` continues to be where new work happens before it's merged back here.

The sketch is deliberately named **without a version number** (renamed from `RS700_Demo_Rev2_0` in R2.1-3) — version history lives in git commits and tags (`v2.0`, `v6-production-1`, …), not in the filename. "Which revision is this" is answered by `git log`/`git tag`, not by what folder you're looking at.

| | Active production | Production v1 (historical) |
|---|---|---|
| Sketch | `RS700_Demo/RS700_Demo.ino` | `RS700_Demo_rev_6` |
| Status | Active, on `main` — Rev 2.1, confirmed working on a full 4-module hardware chain, tagged `v2.1` | Frozen August 2026, tagged `v6-production-1` |
| Changes allowed | Yes, on `Experiment` then merged in | No — kept as an untouched historical reference |

`RS700_Demo_rev_6` is kept in the repo unchanged as the frozen baseline the current firmware was built from and diffed against — **do not edit it**. A unit already in the field can always be reflashed with exactly `v6-production-1` if needed.

Note that the active sketch uses a proper Arduino sketch folder (`RS700_Demo/RS700_Demo.ino`) so it opens directly in the Arduino IDE, unlike the extensionless `RS700_Demo_rev_6`.

The previous contents of this branch (the pre-production sketches `Led_strip_lava_v3.ino`, `Led_strip_lava_v5.ino`, `RS700_Demo_1.ino`, `RS700_Demo_2.ino`) are preserved in the tag **`experiment-legacy-2026-04`**.

---

## Changes in Rev 2.0

| ID | Change | Status |
|---|---|---|
| R2.0-1 | Debounce on **both edges** of TTL IN. Originally only the falling edge was protected; a code review found the rising edge (pulse end) had no debounce at all, so a noise glitch shortly after a genuine falling edge could end the width measurement early. Fixed by requiring 50 ms of quiet since the last edge (either direction) before any edge is acted on | Tested OK on hardware (TTL IN exercised via R2.0-8 testing) |
| R2.0-2 | New **DEMO mode**: long press (≥ 1000 ms) from OFF/DONE ignites all modules instantly in purple, skipping the sequential fill — reuses the existing Fire2012 flicker in its "full flame" state. No timeout; stopped the same way as PLAYING, with a long press (same 5 s fade-out). Final tuned color: see R2.0-11 | Tested — see R2.0-5 |
| R2.0-3 | `NUM_PIXELS`/`USED_LEDS` raised from 263 to 272 (largest module seen so far), `getLedsForRing()` simplified to a flat 16/ring (272 = 17×16 exactly). One firmware image runs on modules with fewer physical LEDs — the NeoPixel chain simply doesn't deliver data past the last real LED, so a shorter module just has an incomplete outermost ring, nothing breaks | Tested OK on hardware |
| R2.0-4 | Burn-up timeline halved proportionally: `FILL_DURATION_SEC` 10→5, `FULL_FLAME_END_SEC` 16→8, `FADE_END_SEC` 30→15, `BLEND_DURATION` 6→3. `TTL_START_PULSE_SEC`'s offset also scaled (1.0s→0.5s) to keep the inter-module trigger at the same relative point in the flame | Tested OK on hardware |
| R2.0-5 | Fixed DEMO's purple render: `heatToColor()` is tuned for a fire palette (blue held near 0), which produced a dark/wrong color when given a purple base whose blue channel dominates. Added `heatToColorUniform()` — scales all three channels proportionally with heat — used only for DEMO | Tested OK on hardware |
| R2.0-6 | Fixed long-press re-arming: the button was re-armed based only on time since the last event (`BTN_DEBOUNCE_MS`), not on whether it had actually been physically released. A continuous hold of more than ~1.5s could trigger the long-press action multiple times in a row (e.g. start DEMO, then immediately stop it again) without the button ever being released. The action still fires the instant hold time crosses `ON_MAX_MS` while still held, but a `buttonLongActionFired` guard now blocks re-triggering until an actual release | Tested OK on hardware |
| R2.0-7 | `lastButtonEventMs`/`lastTtlHandledEventMs` initialized to `-debounce_time` (intentional unsigned underflow) instead of `0`, so a press/pulse in the first few milliseconds after boot isn't silently dropped by the debounce check | Tested OK on hardware |
| R2.0-8 | DEMO (start/stop on long press) was only wired to the M5Atom's onboard button (GPIO39), never to TTL IN. On modules where the physical test button is actually wired through TTL IN rather than GPIO39, DEMO could never trigger. The same long-press logic for DEMO is now mirrored in the TTL IN handling | Tested OK on hardware — this was the actual root cause of "DEMO never starts" |
| R2.0-9 | DEMO now propagates to the next module in the TTL chain immediately on start/stop, mirroring the existing `beginStartPulseOut()`/`beginStopPulseOut()` pattern used for PLAYING/STOP — without this, only the module that received the trigger would enter DEMO, not the rest of the chain | Propagation direction was right, pulse type was wrong — see R2.1-1 |
| R2.0-10 | DEMO ran at full strip brightness with no dimming (unlike the sine-pulsed glow state in normal mode) and looked too intense. Added `DEMO_BRIGHTNESS_SCALE` to dim DEMO specifically, tunable independently of the base color | Tested OK on hardware |
| R2.0-11 | DEMO color tuned live against real hardware through several iterations: `#9255C0` (original) → `#6E4090` → `#4A2B60`, converging on **`#8C0FD2`** (`DEMOPURPLE_R/G/B` = 140, 15, 210) at `DEMO_BRIGHTNESS_SCALE = 1.0`. Darker attempts combined with brightness dimming stacked two dampenings and pushed per-pixel channel values low enough that WS2812 color mixing skewed toward red/grey at typical (non-spark) heat levels — fixed by keeping the base color itself as the only lever and cutting green sharply (green reads disproportionately bright to the eye at low PWM) while boosting red enough to read as purple rather than plain blue. All four tried colors are kept as commented alternatives in the source for quick A/B swaps | Tested OK on hardware |

## Changes in Rev 2.1

| ID | Change | Status |
|---|---|---|
| R2.1-1 | Fixed DEMO chain propagation: with all 4 modules connected, module 1 correctly went purple but modules 2–4 lit up in **normal red** instead of DEMO. Root cause: R2.0-9 propagated DEMO using the same **short** pulse (`beginStartPulseOut()`, 500 ms) as normal START — but a receiving module only distinguishes short-vs-long by pulse width, so it read that as "start normal sequence," having no way to know the sender was in DEMO. Fixed by having `startDemoSequence()` send the **long** pulse instead (renamed `beginStopPulseOut()` → `beginLongPulseOut()`, `TTL_STOP_PULSE_MS` → `TTL_LONG_PULSE_MS`), since a receiver already correctly interprets a long pulse arriving while OFF/DONE as "start DEMO" (R2.0-8) — the same wire signal now means STOP-if-PLAYING, STOP-DEMO-if-DEMO, or START-DEMO-if-idle, decided entirely by the receiver's own mode. **Known, accepted trade-off:** since a receiver needs the pulse held ≥1000 ms to recognize it as "long," DEMO cascades down a 4-module chain as a ~1 s/hop wave (module 4 lights ~3 s after module 1) rather than instantly — reducing this further would require a separate, faster-recognized signal just for DEMO propagation, decided against as added protocol complexity | Tested OK on hardware — all 4 modules confirmed entering DEMO with the expected cascade |
| R2.1-2 | DEMO now fills one module over `DEMO_FILL_DURATION_SEC` (1.5 s) — same fill mechanic as normal ignition (organic per-column ramp via `useFillMask`), just much faster — instead of jumping straight to full flame. Combined with the ~1 s/hop cascade (R2.1-1), this smooths the wave down the chain instead of each module snapping stepwise from dark to full purple | Tested on hardware — fill phase confirmed, but rendered wrong color during the climb, see R2.1-4 |
| R2.1-4 | Fixed wrong color during DEMO's fill phase: R2.1-2's `useFillMask` branch called `heatToColor()` unconditionally — the same fire-palette bug R2.0-5 fixed for DEMO's steady-state render, but missed here since it's a separate code path reused from normal ignition. Result: LEDs rendered reddish (fire palette suppresses blue) while a module was climbing, correcting to purple only once the fill completed. Fixed with the same `demoActive` check used in the steady-state branch, routing to `heatToColorUniform()` during fill too | Tested OK on hardware |
| R2.1-5 | DEMO-start propagation fired instantly in `startDemoSequence()`, so a downstream module could start its own fill before the upstream module had gotten meaningfully underway — the 1.5 s fill duration (R2.1-2) and the ≥1000 ms a receiver needs to recognize a long pulse as "long" were never coordinated. Fixed by deferring the outgoing pulse `DEMO_PROPAGATE_DELAY_SEC` (0.5 s) into the fill phase instead of sending it immediately — the same principle `TTL_START_PULSE_SEC` already applies to normal ignition | Tested OK on hardware |
| R2.1-6 | Stopping DEMO only turned off the module that received the stop press — the rest of the chain stayed lit in DEMO, unlike normal STOP which propagates correctly. Root cause: R2.1-1 unified START-DEMO and STOP-DEMO propagation onto the same `ttlLongPulseTriggered` latch. Start propagation (R2.1-5) sets it true; `stopDemoSequence()` never reset it before its own `beginLongPulseOut()` call, so that function's own guard silently dropped the stop pulse. Normal PLAYING→STOP is unaffected since START and STOP there use two separate latches (`ttlStartPulseTriggered` vs `ttlLongPulseTriggered`). Fixed by resetting `ttlLongPulseTriggered` in `stopDemoSequence()` before sending | Tested OK on hardware |

First hardware test (2026-08-17): button, normal ignition, LED count and halved timing all confirmed working; DEMO mode did not trigger at all initially. Root-caused through several rounds (color bug, button re-arm bug, boot-window debounce bug) to R2.0-8: the physical test button on this rig is wired through TTL IN, not the onboard M5Atom button. A later "still doesn't work" report turned out to be a stale Arduino IDE editor buffer overwriting the file on save, and a subsequent "SW regression" report turned out to be two boards running different firmware builds (one an older, never-updated flash) rather than an actual code difference — both resolved by reflashing consistently and adopting a habit of fully closing/reopening the sketch in the IDE before every compile. All fixes above (R2.0-1 through R2.0-11) now confirmed working on hardware.

Second hardware test (2026-08-19), full 4-module chain: R2.1-1's propagation fix and R2.1-2's fill phase both confirmed working — all 4 modules enter DEMO with the expected cascade and fill. Found in the same session: (1) LEDs turned reddish while a module's DEMO fill was climbing, not yet visible in R2.0-11's steady-state testing since that only exercised the "already full" render path — fixed as R2.1-4; (2) stopping DEMO only turned off the module that received the press, the rest of the chain stayed lit — fixed as R2.1-6; (3) the cascade to the next module started noticeably before the current module's fill was done — fixed as R2.1-5. Re-flashed and re-tested on the same 4-module chain same day: all three fixes confirmed working correctly. **Rev 2.1 (R2.0-1 through R2.1-6) is now confirmed production-ready on hardware, tagged `v2.1`.**

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

Reachable via **either** the M5Atom's onboard button (GPIO39) **or** TTL IN (pin 21, active LOW) — both inputs are wired to the exact same state machine, so on rigs where the physical button is actually connected through TTL IN rather than GPIO39, everything below still applies.

| Action | From mode | Result |
|---|---|---|
| Short press (< 1000 ms) | OFF / DONE | START sequence (normal red/orange burn-up) |
| Long press (≥ 1000 ms) | PLAYING | STOP — 5 s fade-out, then DONE |
| Long press (≥ 1000 ms) | OFF / DONE | **DEMO** — instant purple "full flame" on all modules, no timeout, propagated to the next module in the TTL chain |
| Long press (≥ 1000 ms) | DEMO | Stop DEMO — same 5 s fade-out, then DONE, propagated to the next module |

Any press while `FADING` or while already in the target mode has no effect. Debounce between button events: 500 ms. Long-press actions fire immediately once the hold crosses 1000 ms (not on release).

---

## TTL protocol (module synchronization)

Four modules are connected in series. Module 1 is master and sends TTL pulses to downstream modules. TTL IN drives the exact same state machine as the physical button (see [Ignite button](#ignite-button)) — including DEMO.

| Pulse width | From mode | Meaning |
|---|---|---|
| < 1000 ms LOW | OFF / DONE | START |
| ≥ 1000 ms LOW | PLAYING | STOP |
| ≥ 1000 ms LOW | OFF / DONE | DEMO |
| ≥ 1000 ms LOW | DEMO | Stop DEMO |

- Idle: HIGH
- Active: LOW
- Debounce: 50 ms (falling and rising edge)
- Long-pulse actions fire the instant held time crosses 1000 ms, not on release — mirroring the button
- **Outgoing propagation pulses:** a module sends a 500 ms pulse (`TTL_START_PULSE_MS`) to propagate normal START, and a 2000 ms pulse (`TTL_LONG_PULSE_MS`) to propagate STOP, STOP-DEMO, or START-DEMO — the receiving module decides which of the three based on its own current mode, not on anything the sender encodes. Since a receiver needs ≥1000 ms of a held pulse to recognize it as "long," DEMO cascades down the chain as a wave rather than instantly (see R2.1-1). The START-DEMO pulse is deferred `DEMO_PROPAGATE_DELAY_SEC` (0.5 s) into the sending module's own fill phase rather than sent the instant DEMO begins, so a downstream module doesn't start filling before the upstream one has gotten meaningfully underway (see R2.1-5)

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

**DEMO mode** (long press from OFF/DONE, button or TTL IN) skips this whole timeline: it locks into the "full flame" parameters permanently, recolored purple (`#8C0FD2`, see R2.0-11), with no color/glow progression and no auto-off — it stays until stopped with another long press. Uses `heatToColorUniform()` rather than `heatToColor()` since the latter is fire-tuned and suppresses blue, a dominant channel in this purple.

### Modifications from Fire2012

- Ported from FastLED to Adafruit_NeoPixel
- 2D column-based `heat[FIRE_COLS][RINGS]` instead of 1D
- Per-column `colOffset[]` and `colRotation[]` for organic variation
- Custom `heatToColor()` with red/orange lava palette instead of FastLED's `HeatColors`
- `heatToColorUniform()` — proportional (non-fire-biased) channel scaling, used only for DEMO's purple
- `lavaFlicker()` — deterministic pseudorandom glow effect based on LED index and time
- `baseColorForTime()` — time-driven color palette shift via smoothstep interpolation
- `fadeScale` — global brightness ramp-down during FADING state
- TTL in/out with ISR and debounce for multi-module synchronization, mirrored on the button
- Button/TTL IN: short press = START, long press (≥ 1 s) = STOP or DEMO depending on mode
