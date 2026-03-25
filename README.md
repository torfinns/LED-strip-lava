# LED-strip-lava

TODO:
1. Print toppanel
2. Koble kabler
3. Koble PeliCase
4. Skjema for Tool Upper
5. Implementer logikken for sekvensiell styring
6. Koble opp det vi har for demo


RULES:
1. BRUK 3S på alle 263 LEDs. De skal blafre
2. Send ut TTL signal til neste modul
3. Etter 4,5 s start fargeskift
4. Fargeskift: (gradvis / mange nyanser)
a) Mørk rød (4,5s)
b) Lys rød (4,5s)
c) Orange (til ferdig)
6. Blafring:
- Lineær, ikke sirkulær
- Lava-effekt / gløde-effekt
8. Timing:
- Etter 3x4 + 2x 4,5 = 21 s: alle LED / power orange
- 40s: Alle mørke

Rekapitulering:
-BRUK 3s på alle 263 LEDs. De skal starte som mørk røde, blafre opp langs sylinderen i lineære bevegelser som glødende lava i rødt / orange på tur opp langs sylinderen
-Etter 4,5 s start et glidende fargeskift (som simulerer stadig varmere metall) fra mørk rødt til mørk orange. Bruk ca 9s på overgangen fra mørk rød til mørk orange. Gå innom lysere rød og de nærliggende nyansene på veien.
-Når alle LEDs er mørk orange skal de likevel gløde som lava rundt nyanser i rødt og orange
-Etter 40s: Alle mørke igjen

Rotated RGB LED strip controlled by M5Atom for lava / burning effect

Setup, installation, board selection and libraries:
Arduino IDE 1.8.19 – ESP32 support:
Open File → Preferences.
In “Additional Boards Manager URLs” add: https://espressif.github.io/arduino-esp32/package_esp32_index.json Click OK.
Go to Tools → Board → Boards Manager….
Search for “esp32” and install “ESP32 by Espressif Systems”.
Select board: Tools → Board → ESP32 Arduino → ESP32 Dev Module.
Select port: Tools → Port → COM11 (or whichever port the Atom is on).
Set Tools → Upload Speed → 115200 for reliable uploads.

Required libraries
In Sketch → Include Library → Manage Libraries…:
Install “Adafruit NeoPixel” (for the LED strip).
Install “M5Atom” (for the Atom Lite button and internal LED).

Physical wiring (current setup)
Board: M5Stack Atom Lite (ESP32‑PICO‑D4).

LED strip: Adafruit 1507, 144 LEDs (in this project we effectively treat it as 8 rings × 18 LEDs).

Wiring:
White JST wire → GPIO 22 (data).
Black JST wire → GND.
Red loose wire → 5V on the Atom (safe here because brightness is low; for full 144‑LED power use a separate 5 V supply).
Black loose wire → GND (same ground as the Atom).


  RS700 Cylinder Fire Effect
  --------------------------

  Based on "Fire2012" by Mark Kriegsman (FastLED project):
  https://github.com/FastLED/FastLED/blob/master/examples/Fire2012/Fire2012.ino
  and the accompanying article:
  https://blog.kriegsman.org/2014/04/04/fire2012-an-open-source-fire-simulation-for-arduino-and-leds/

  Core algorithm:
  - Uses a 1D heat[] array along the vertical axis.
  - Each frame:
    1) COOLING: each cell cools down a little.
    2) HEAT DIFFUSION: heat drifts upward and is blurred.
    3) SPARKING: random new heat is added near the bottom.
  - The heat values are mapped to color using a custom HeatColor8()
    to create a lava-like red/orange palette.

  Modifications in this sketch:
  - Ported from FastLED (CRGB / FastLED.show) to Adafruit_NeoPixel
    (strip.setPixelColor / strip.show) on an M5Atom (ESP32).
  - The strip is treated as a cylinder: 8 rings × 18 LEDs each.
    A serpentine XY() mapping converts (ring, height) to the 1D index.
  - Temporal control:
    * spreadSeconds: how long the fire front takes to move from ring 0
      around to the last ring.
    * holdSeconds: how long all rings remain fully lit before the effect ends.
  - Per-ring fade-in:
    * ringFade[ring] ramps from 0 → 255 once a ring becomes active,
      so new rings fade in smoothly instead of turning on abruptly.
  - Reduced flicker behind the fire front:
    * flickerFactorForRing() reduces the apparent flicker on rings
      several steps behind the leading edge, making them look more like
      steady glowing embers/lava.

