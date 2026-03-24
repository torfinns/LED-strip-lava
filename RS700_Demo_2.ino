#include <M5Atom.h>
#include <Adafruit_NeoPixel.h>

// ---------- HW-KONFIG ----------
// ADVARSEL: 263 LEDs ved BRIGHTNESS 100 trekker ~1.6A (rød/oransje).
// Full hvit ved 255 = ~15A. Bruk ekstern 5V strømforsyning!
#define PIN        22
#define NUM_PIXELS 263
// 0-255. 0 = av, 255 = maks. Anbefalt maks 100 uten kraftig PSU.
#define BRIGHTNESS 100

static_assert(NUM_PIXELS >= 263, "NUM_PIXELS er for lavt");

Adafruit_NeoPixel strip(NUM_PIXELS, PIN, NEO_GRB + NEO_KHZ800);

// ---------- Sylinder-geometri ----------
// Ror: OD 32 mm, ledningsspor spiralviklet
// 17 rotasjoner: 8 ringer × 16 LEDs + 9 ringer × 15 LEDs = 263 totalt
const uint8_t  RINGS         = 17;
const uint16_t USED_LEDS     = 263;

static_assert(USED_LEDS <= NUM_PIXELS, "USED_LEDS overstiger NUM_PIXELS");

// ---------- Fire2012-parametre ----------
#define COOLING  65   // 0-100, hoyere = kortere/roligere flammer
#define SPARKING 90   // 0-255, hoyere = mer gnister
#define FRAMES_PER_SECOND 60
#define FRAME_MS (1000 / FRAMES_PER_SECOND)

// Gnistsone: overste 1/4 av hoyden (basert på gjennomsnittlig LED-er per ring)
static const uint8_t SPARK_ZONE = 4;  // ~1/4 av 15-16 LEDs

// Terskelgrenser for HeatColor8
static const uint8_t HEAT_HIGH = 0x80;
static const uint8_t HEAT_MID  = 0x40;

// Hoydeprofil for flamme (langs sylinderen) - maks 16 LEDs per ring
uint8_t g_heat[16];

// Fade-niva per ring (0-255)
uint8_t g_ringFade[RINGS];

// Hvor langt bak flammekanten det skal flimre tydelig
const uint8_t FLICKER_TRAIL_RINGS = 5;

// ---------- Tidsstyring ----------
const float spreadSeconds = 2.0f;  // sek fra ring 0 til siste ring
const float holdSeconds   = 20.0f; // sek alle ringer brenner

enum Mode { OFF, PLAYING, DONE };
Mode mode = OFF;

uint32_t startMillis = 0;
uint32_t lastFrameMs = 0;

// ---------- Fremover-deklarasjoner ----------
void FireStep();
uint32_t HeatColor8(uint8_t temperature);
uint16_t XY(uint8_t ring, uint8_t y);
float flickerFactorForRing(int8_t ring, uint8_t activeRings);
void clearStrip();
uint8_t calcActiveRings(float t);
uint8_t getLedsForRing(uint8_t ring);

// ================== SETUP ==================
void setup() {
  M5.begin(true, false, true);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();

  memset(g_heat,     0, sizeof(g_heat));
  memset(g_ringFade, 0, sizeof(g_ringFade));
}

// ================== LOOP ===================
void loop() {
  M5.update();

  if (M5.Btn.wasPressed()) {
    if (mode == OFF || mode == DONE) {
      startMillis = millis();
      lastFrameMs = startMillis;
      mode = PLAYING;
      memset(g_heat,     0, sizeof(g_heat));
      memset(g_ringFade, 0, sizeof(g_ringFade));
    } else if (mode == PLAYING) {
      mode = OFF;
      clearStrip();
    }
  }

  uint32_t now = millis();
  if ((now - lastFrameMs) < FRAME_MS) return;  // ikke-blokkerende FPS-begrensning
  lastFrameMs = now;

  if (mode == PLAYING) {
    float t = (now - startMillis) / 1000.0f;

    uint8_t activeRings = calcActiveRings(t);

    if (activeRings == 0) {
      clearStrip();
      mode = DONE;
      return;
    }

    // Oppdater fade-inn per ring
    for (uint8_t r = 0; r < RINGS; r++) {
      if (r < activeRings) {
        uint16_t val = g_ringFade[r] + 10;
        g_ringFade[r] = (val > 255) ? 255 : (uint8_t)val;
      } else {
        g_ringFade[r] = 0;
      }
    }

    // Oppdater flammen langs hoyden
    FireStep();

    // Tegn til alle ringer med fade + flimmer-demping
    for (uint8_t ring = 0; ring < RINGS; ring++) {
      uint8_t fade = g_ringFade[ring];
      float flickerFactor = flickerFactorForRing(ring, activeRings);
      uint8_t ledsInRing = getLedsForRing(ring);

      for (uint8_t y = 0; y < ledsInRing; y++) {
        uint32_t color = 0;

        if (fade > 0) {
          // Bruk heat-verdi scaled til denne ringens oppløsning
          uint8_t h = g_heat[(y * 16) / ledsInRing];

          uint8_t smoothed = h;
          if (flickerFactor < 1.0f) {
            smoothed = (uint8_t)((float)h * flickerFactor
                                 + 128.0f * (1.0f - flickerFactor));
          }

          uint32_t base = HeatColor8(smoothed);
          uint8_t r8 = (uint8_t)((base >> 16) & 0xFF);
          uint8_t g8 = (uint8_t)((base >> 8)  & 0xFF);
          uint8_t b8 = (uint8_t)(base & 0xFF);

          r8 = (uint8_t)((uint16_t)r8 * fade / 255);
          g8 = (uint8_t)((uint16_t)g8 * fade / 255);
          b8 = (uint8_t)((uint16_t)b8 * fade / 255);

          color = strip.Color(r8, g8, b8);
        }

        uint16_t idx = XY(ring, y);
        if (idx != UINT16_MAX && idx < USED_LEDS) {
          strip.setPixelColor(idx, color);
        }
      }
    }

    strip.show();
  }
}

// ---------- Beregn antall aktive ringer ----------
uint8_t calcActiveRings(float t) {
  if (t <= spreadSeconds) {
    float perRing = spreadSeconds / RINGS;
    uint8_t rings = (uint8_t)(t / perRing) + 1;
    return (rings > RINGS) ? RINGS : rings;
  } else if (t <= spreadSeconds + holdSeconds) {
    return RINGS;
  }
  return 0;  // animasjonen er ferdig
}

// ---------- Slukk alle LEDs ----------
void clearStrip() {
  for (uint16_t i = 0; i < USED_LEDS; i++) {
    strip.setPixelColor(i, 0);
  }
  strip.show();
}

// ---------- Fire2012 langs hoyden ----------
void FireStep() {
  for (int i = 0; i < 16; i++) {
    uint8_t cooldown = random(0, ((COOLING * 10) / 16) + 2);
    g_heat[i] = (cooldown > g_heat[i]) ? 0 : g_heat[i] - cooldown;
  }

  for (int k = 15; k >= 2; k--) {
    g_heat[k] = (g_heat[k - 1] + g_heat[k - 2] + g_heat[k - 2]) / 3;
  }

  if (random(0, 255) < SPARKING) {
    int y = random(0, SPARK_ZONE);
    uint8_t added = random(160, 255);
    uint16_t h = g_heat[y] + added;
    g_heat[y] = (h > 255) ? 255 : (uint8_t)h;
  }
}

// ---------- Lava-aktig HeatColor ----------
uint32_t HeatColor8(uint8_t temperature) {
  uint8_t t192 = (uint8_t)(((uint16_t)temperature * 191) / 255);
  uint8_t heatramp = (t192 & 0x3F) << 2;

  uint8_t r, g, b;

  if (t192 > HEAT_HIGH) {       // glodende topp
    r = 255;
    g = heatramp / 4;
    b = 0;
  } else if (t192 > HEAT_MID) { // midt
    r = 220;
    g = heatramp / 6;
    b = 0;
  } else {                      // bunn
    r = heatramp;
    g = 0;
    b = 0;
  }

  return strip.Color(r, g, b);
}

// ---------- Hent antall LEDs for en gitt ring ----------
uint8_t getLedsForRing(uint8_t ring) {
  if (ring < 8) {
    return 16;  // Første 8 ringer: 16 LEDs hver
  } else {
    return 15;  // Siste 9 ringer: 15 LEDs hver
  }
}

// ---------- Mapping: ring/y -> LED-index ----------
uint16_t XY(uint8_t ring, uint8_t y) {
  if (ring >= RINGS) return UINT16_MAX;
  
  uint8_t ledsThisRing = getLedsForRing(ring);
  if (y >= ledsThisRing) return UINT16_MAX;

  // Beregn base-indeks (hvor denne ringen starter)
  uint16_t base = 0;
  for (uint8_t r = 0; r < ring; r++) {
    base += getLedsForRing(r);
  }

  // Serpentin: annenhver ring snur retning
  return (ring % 2 == 0)
    ? base + y
    : base + (ledsThisRing - 1 - y);
}

// ---------- Flimmer-demping per ring ----------
float flickerFactorForRing(int8_t ring, uint8_t activeRings) {
  int8_t headRing = (int8_t)activeRings - 1;
  int8_t dist = headRing - ring;

  if (dist <= 0) return 1.0f;
  if (dist >= FLICKER_TRAIL_RINGS) return 0.1f;

  float t = (float)dist / (float)FLICKER_TRAIL_RINGS;
  return 1.0f - 0.9f * t;
}
