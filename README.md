# LED-strip-lava

RGB LED-strip kontrollert av M5Atom Lite for lava/branneffekt. Brukes i RS700 Demo-installasjon med fire moduler i rekke, synkronisert via TTL-signaler.

---

## Oppsett, installasjon, kortvalgog biblioteker

### Arduino IDE 2.x – ESP32-støtte

1. Åpne **File → Preferences**.
2. I feltet "Additional Boards Manager URLs", legg til:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Gå til **Tools → Board → Boards Manager…**, søk etter "esp32" og installer **ESP32 by Espressif Systems**.
4. Velg kort: **Tools → Board → ESP32 Arduino → M5Stack-ATOM**.
5. Velg port: **Tools → Port → COMxx** (eller aktuell port).
6. Sett **Tools → Upload Speed → 115200** for stabil opplasting.

### Påkrevde biblioteker

I **Sketch → Include Library → Manage Libraries…**:

- **Adafruit NeoPixel** – for LED-stripen
- **M5Atom** – for Atom Lite-knapp og intern LED

---

## Fysisk tilkobling

**Kort:** M5Stack Atom Lite (ESP32‑PICO‑D4)

**LED-strip:** 263 adresserbare RGB-LEDs fordelt på 17 ringer (ring 0–7: 16 LEDs, ring 8–16: 15 LEDs)

| Signal | GPIO | Kabel |
|---|---|---|
| LED data | 22 | Hvit JST |
| GND | GND | Svart JST |
| 5V | 5V | Separat 5V forsyning anbefalt ved full lysstyrke |
| TTL ut | 19 | Aktiv LOW, hvile HIGH |
| TTL inn | 21 | Aktiv LOW, intern pullup, INPUT_PULLUP |

**Merk:** Felles GND mellom alle moduler er påkrevd for korrekt TTL-kommunikasjon.

---

## TTL-protokoll (modulsynkronisering)

Fire moduler kobles i rekke. Modul 1 er master og sender TTL-pulser til etterfølgende moduler.

| Pulsbredde | Betydning |
|---|---|
| < 1000 ms LOW | START-signal |
| ≥ 1000 ms LOW | STOPP-signal |

- Hvile: HIGH
- Aktiv: LOW
- Debounce: 50 ms (falling og rising edge)

---

## RS700 Cylinder Fire Effect

### Opprinnelse

Algoritmen er basert på **Fire2012** av Mark Kriegsman (FastLED-prosjektet):
- Kode: https://github.com/FastLED/FastLED/blob/master/examples/Fire2012/Fire2012.ino
- Artikkel: https://blog.kriegsman.org/2014/04/04/fire2012-an-open-source-fire-simulation-for-arduino-and-leds/

Fra Fire2012 er følgende beholdt konseptuelt:
- `heat[]`-array langs vertikal akse
- Tre-stegs per-frame-logikk: **COOLING → HEAT DIFFUSION → SPARKING**
- `COOLING`- og `SPARKING`-parametere

Implementasjonen er ellers skrevet fra bunnen av og benytter **Adafruit NeoPixel** i stedet for FastLED. `qsub8`/`qadd8`/`random8` er de eneste FastLED-hjelpefunksjonene som er beholdt direkte.

### Geometri

Stripen behandles som en sylinder med 17 ringer. En `FLAME_XY(ring, col)`-funksjon mapper (ring, kolonne) til lineær LED-indeks. Ringen bruker ikke serpentine-mapping — alle ringer er adressert lineært fra `ringBase(ring)`.

### Animasjonssekvens

| Fase | Tid | Beskrivelse |
|---|---|---|
| Fill | 0 – 10 s | Ilden fyller sylinderen ring for ring nedenfra og opp |
| Full flame | 10 – 16 s | Full branneffekt med COOLING=40, SPARKING=120 |
| Color shift | 16 – 30 s | Fargen blender fra mørk rød → oransje via smoothstep |
| Blend to glow | 30 – 36 s | Brannalgoritmen fases ut, lavaflimmer fases inn |
| Lava glow | 36 s – | Stasjonær lavaglød med sinusbasert pulsering |
| All off | 300 s | Sekvensen stoppes automatisk |

### Modifikasjoner fra Fire2012

- Portert fra FastLED til Adafruit_NeoPixel
- 2D kolonne-basert `heat[FIRE_COLS][RINGS]` i stedet for 1D
- Per-kolonne `colOffset[]` og `colRotation[]` for organisk variasjon
- Egendefinert `heatToColor()` med rød/oransje lavapalett i stedet for FastLEDs `HeatColors`
- `lavaFlicker()` — deterministisk pseudorandom glødeffekt basert på LED-indeks og tid
- `baseColorForTime()` — tidsstyrt fargepalettskift via smoothstep-interpolasjon
- `fadeScale` — global nedtoning ved FADING-tilstand
- TTL inn/ut med ISR og debounce for multi-modul synkronisering
- Knapp: kort trykk = START, langt trykk (≥ 1 s) = STOPP
