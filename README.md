# LED-strip-lava

RGB LED strip controlled by M5Atom Lite for lava/fire effect. Used in the RS700 Demo installation with four modules in series, synchronized via TTL signals.

---

## TODO

### Pending robustification measures

| Measure | Type | Risk | Interferes with code |
|---|---|---|---|
| Debounce on falling edge | SW | None | Minimal change, one line |
| RC filter 100 Ω + 100 nF on TTL_IN_PIN | HW | None | No |
| External 10 kΩ pullup to 3.3 V | HW | None | No |

### Debounce fix — falling edge (TTL IN)

Add `lastTtlHandledEventMs = now` on falling edge. Currently this timestamp is only updated on rising edge, leaving falling edge unprotected against noise and contact bounce.

Location: TTL IN handling block in `loop()`, approximately **line 272**.

Current code:
```cpp
if (!level) {
  // FALLING: active pulse starts
  if (now - lastTtlHandledEventMs >= TTL_DEBOUNCE_MS) {
    ttlMeasuredActive = true;
    ttlActiveStartMs  = now;
  }
}
```

Fixed — add one line:
```cpp
if (!level) {
  // FALLING: active pulse starts
  if (now - lastTtlHandledEventMs >= TTL_DEBOUNCE_MS) {
    ttlMeasuredActive     = true;
    ttlActiveStartMs      = now;
    lastTtlHandledEventMs = now;  // <-- add this line
  }
}
```

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

**LED strip:** 263 addressable RGB LEDs arranged in 17 rings (rings 0–7: 16 LEDs each, rings 8–16: 15 LEDs each)

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

| Action | Duration | Result |
|---|---|---|
| Short press | < 1000 ms | START sequence (from OFF or DONE only) |
| Long press | ≥ 1000 ms | STOP sequence (from PLAYING only) |

Debounce between button events: 500 ms. STOP triggers a 5-second fade-out before the system enters DONE state.

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
| Fill | 0 – 10 s | Fire fills the cylinder ring by ring, bottom to top |
| Full flame | 10 – 16 s | Full fire effect, COOLING=40, SPARKING=120 |
| Color shift | 16 – 30 s | Color blends from dark red → orange via smoothstep |
| Blend to glow | 30 – 36 s | Fire algorithm fades out, lava flicker fades in |
| Lava glow | 36 s – | Steady lava glow with sine-based brightness pulsing |
| All off | 300 s | Sequence stops automatically |

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
