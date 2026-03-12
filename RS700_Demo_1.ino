#include <M5Atom.h>
#include <Adafruit_NeoPixel.h>

// ---------- HW-KONFIG ----------
#define PIN        22
#define NUM_PIXELS 144
// 0–255. 0 = av, 255 = maks. Praktisk 20–100.
#define BRIGHTNESS 200

Adafruit_NeoPixel strip(NUM_PIXELS, PIN, NEO_GRB + NEO_KHZ800);

// ---------- Sylinder-geometri ----------
const uint8_t  RINGS         = 8;    // 8 rotasjoner rundt sylinderen
const uint8_t  LEDS_PER_RING = 18;   // 8 * 18 = 144
const uint16_t USED_LEDS     = RINGS * LEDS_PER_RING; // 144 i bruk

// ---------- Fire2012-parametre ----------
#define COOLING  65   // 0–100, høyere = kortere/roligere flammer
#define SPARKING 90   // 0–255, høyere = mer gnister
#define FRAMES_PER_SECOND 60

// Høydeprofil for flamme (langs sylinderen)
uint8_t heat[LEDS_PER_RING];

// Fade-nivå per ring (0–255)
uint8_t ringFade[RINGS];

// Hvor langt bak flammekanten det skal flimre tydelig
const uint8_t FLICKER_TRAIL_RINGS = 5;

// ---------- Tidsstyring ----------
float spreadSeconds = 2.0;    // sek fra ring 0 til ring 7
float holdSeconds   = 20.0;   // sek alle 8 ringer brenner

enum Mode { OFF, PLAYING, DONE };
Mode mode = OFF;

unsigned long startMillis = 0;

// ---------- Fremover-deklarasjoner ----------
void FireStep();
uint32_t HeatColor8(uint8_t temperature);
uint16_t XY(uint8_t ring, uint8_t y);
float flickerFactorForRing(int8_t ring, uint8_t activeRings);

// ================== SETUP ==================
void setup() {
  M5.begin(true, false, true);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();

  for (uint8_t i = 0; i < LEDS_PER_RING; i++) {
    heat[i] = 0;
  }
  for (uint8_t r = 0; r < RINGS; r++) {
    ringFade[r] = 0;
  }
}

// ================== LOOP ===================
void loop() {
  M5.update();

  if (M5.Btn.wasPressed()) {
    if (mode == OFF || mode == DONE) {
      startMillis = millis();
      mode = PLAYING;
      for (uint8_t i = 0; i < LEDS_PER_RING; i++) heat[i] = 0;
      for (uint8_t r = 0; r < RINGS; r++) ringFade[r] = 0;
    } else if (mode == PLAYING) {
      mode = OFF;
      for (uint16_t i = 0; i < USED_LEDS; i++) {
        strip.setPixelColor(i, 0, 0, 0);
      }
      strip.show();
    }
  }

  if (mode == PLAYING) {
    unsigned long now = millis();
    float t = (now - startMillis) / 1000.0;

    float   perRing     = spreadSeconds / RINGS;
    uint8_t activeRings = 0;

    if (t >= 0 && t <= spreadSeconds) {
      uint8_t ringIndex = (uint8_t)(t / perRing) + 1;
      if (ringIndex > RINGS) ringIndex = RINGS;
      activeRings = ringIndex;
    } else if (t > spreadSeconds && t <= spreadSeconds + holdSeconds) {
      activeRings = RINGS;
    } else {
      for (uint16_t i = 0; i < USED_LEDS; i++) {
        strip.setPixelColor(i, 0, 0, 0);
      }
      strip.show();
      mode = DONE;
      delay(1000 / FRAMES_PER_SECOND);
      return;
    }

    // Oppdater fade-inn per ring
    for (uint8_t r = 0; r < RINGS; r++) {
      if (r < activeRings) {
        uint16_t val = ringFade[r] + 10;  // juster for raskere/saktere fade
        if (val > 255) val = 255;
        ringFade[r] = (uint8_t)val;
      } else {
        ringFade[r] = 0;
      }
    }

    // Oppdater flammen langs høyden
    FireStep();

    // Tegn til alle ringer med fade + flimmer-demping
    for (uint8_t ring = 0; ring < RINGS; ring++) {
      uint8_t fade = ringFade[ring];  // 0–255
      float flickerFactor = flickerFactorForRing(ring, activeRings);

      for (uint8_t y = 0; y < LEDS_PER_RING; y++) {
        uint32_t color = strip.Color(0, 0, 0);
        if (fade > 0) {
          uint8_t h = heat[y];

          // demp flimmer bakover ved å dra heat mot en stabil verdi
          uint8_t smoothed = h;
          if (flickerFactor < 1.0f) {
            smoothed = (uint8_t)( (float)h * flickerFactor
                                  + 128.0f * (1.0f - flickerFactor) );
          }

          uint32_t base = HeatColor8(smoothed);
          uint8_t r = (uint8_t)((base >> 16) & 0xFF);
          uint8_t g = (uint8_t)((base >> 8)  & 0xFF);
          uint8_t b = (uint8_t)(base & 0xFF);

          // skaler med fade (0–255)
          r = (uint8_t)((uint16_t)r * fade / 255);
          g = (uint8_t)((uint16_t)g * fade / 255);
          b = (uint8_t)((uint16_t)b * fade / 255);

          color = strip.Color(r, g, b);
        }

        uint16_t idx = XY(ring, y);
        if (idx < USED_LEDS) {
          strip.setPixelColor(idx, color);
        }
      }
    }

    strip.show();
  }

  delay(1000 / FRAMES_PER_SECOND);
}

// ---------- Fire2012 langs høyden ----------
void FireStep() {
  for (int i = 0; i < LEDS_PER_RING; i++) {
    uint8_t cooldown =
      random(0, ((COOLING * 10) / LEDS_PER_RING) + 2);
    heat[i] = (cooldown > heat[i]) ? 0 : heat[i] - cooldown;
  }

  for (int k = LEDS_PER_RING - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  }

  if (random(0, 255) < SPARKING) {
    int y = random(0, min(7, (int)LEDS_PER_RING));
    uint8_t added = random(160, 255);
    uint16_t h = heat[y] + added;
    heat[y] = (h > 255) ? 255 : h;
  }
}

// ---------- Lava-aktig HeatColor ----------
uint32_t HeatColor8(uint8_t temperature) {
  uint8_t t192 = (uint8_t)((temperature / 255.0) * 191);
  uint8_t heatramp = t192 & 0x3F;
  heatramp <<= 2;

  uint8_t r, g, b;

  if (t192 > 0x80) {           // glødende topp
    r = 255;
    g = heatramp / 4;
    b = 0;
  } else if (t192 > 0x40) {    // midt
    r = 220;
    g = heatramp / 6;
    b = 0;
  } else {                     // bunn
    r = heatramp;
    g = 0;
    b = 0;
  }

  return strip.Color(r, g, b);
}

// ---------- Mapping: ring/y -> LED-index ----------
uint16_t XY(uint8_t ring, uint8_t y) {
  if (ring >= RINGS || y >= LEDS_PER_RING) return 0;

  uint16_t base = (uint16_t)ring * LEDS_PER_RING;

  // serpentin: annenhver ring snur retning
  if (ring % 2 == 0) {
    return base + y;                    // oppover
  } else {
    return base + (LEDS_PER_RING - 1 - y);
  }
}

// ---------- Flimmer-demping per ring ----------
float flickerFactorForRing(int8_t ring, uint8_t activeRings) {
  int8_t headRing = (int8_t)activeRings - 1;
  int8_t dist = headRing - ring; // 0 = flammespiss

  if (dist <= 0) return 1.0f;                         // full flimmer
  if (dist >= FLICKER_TRAIL_RINGS) return 0.1f;       // nesten glød

  float t = (float)dist / (float)FLICKER_TRAIL_RINGS; // 0..1
  return 1.0f - 0.9f * t;                             // 1.0 → 0.1
}
