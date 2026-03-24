#include <M5Atom.h>
#include <Adafruit_NeoPixel.h>

// ---------- HW-KONFIG ----------
#define PIN        22
#define NUM_PIXELS 263
#define BRIGHTNESS 100

static_assert(NUM_PIXELS >= 263, "NUM_PIXELS er for lavt");

Adafruit_NeoPixel strip(NUM_PIXELS, PIN, NEO_GRB + NEO_KHZ800);

// TTL-utgang til neste modul (valgfritt)
#define TTL_PIN 25

// ---------- Sylinder-geometri ----------
const uint8_t  RINGS     = 17;
const uint16_t USED_LEDS = 263;

static_assert(USED_LEDS <= NUM_PIXELS, "USED_LEDS overstiger NUM_PIXELS");

uint8_t getLedsForRing(uint8_t ring) {
  if (ring < 8) return 16;
  else          return 15;
}

uint16_t XY(uint8_t ring, uint8_t y) {
  if (ring >= RINGS) return UINT16_MAX;
  uint8_t ledsThisRing = getLedsForRing(ring);
  if (y >= ledsThisRing) return UINT16_MAX;

  uint16_t base = 0;
  for (uint8_t r = 0; r < ring; r++) {
    base += getLedsForRing(r);
  }
  return (ring % 2 == 0)
    ? base + y
    : base + (ledsThisRing - 1 - y);
}

// ---------- Tidsstyring ----------
#define FRAMES_PER_SECOND 60
#define FRAME_MS (1000 / FRAMES_PER_SECOND)

// Faser (sekunder)
const float fillDuration      = 3.0f;    // 0–3s: fyll opp, mørk rød lava
const float darkRedHoldStart  = 3.0f;    // 3–4.5s: stabil mørk rød lava
const float darkRedHoldEnd    = 4.5f;

const float fadeToOrangeStart = 4.5f;    // 4.5–13.5s: mørk rød -> mørk oransje
const float fadeToOrangeEnd   = 13.5f;   // 9s overgang

const float holdOrangeStart   = 13.5f;   // 13.5–40s: mørk oransje lava
const float allOffTime        = 40.0f;   // 40s: alt av

enum Mode { OFF, PLAYING, DONE };
Mode mode = OFF;

uint32_t startMillis = 0;
uint32_t lastFrameMs = 0;

// ---------- Hjelpefunksjoner ----------
void clearStrip() {
  for (uint16_t i = 0; i < USED_LEDS; i++) {
    strip.setPixelColor(i, 0);
  }
  strip.show();
}

// Lava-blafring rundt en basisfarge (r,g,b)
// posNorm: 0=bunn,1=topp, timeMs for variasjon over tid
// strongFlicker: sterkere variasjon (brukes i starten)
// ledIndex: unik indeks for LED, for mer uavhengig støy
uint32_t lavaFlicker(uint8_t baseR, uint8_t baseG, uint8_t baseB,
                     float posNorm, uint32_t timeMs, bool strongFlicker,
                     uint16_t ledIndex)
{
  // Per-LED støy: bland inn ledIndex, posNorm og tid
  uint32_t seed = (uint32_t)ledIndex * 1103515245u
                + (uint32_t)(posNorm * 1000.0f) * 12345u
                + timeMs / 5;

  // Enkel PRNG fra seed
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  uint8_t noise = (uint8_t)(seed & 0xFF);

  float minLevel   = strongFlicker ? 0.4f : 0.6f;  // 40–100% eller 60–100%
  float flickerAmt = strongFlicker ? 0.4f : 0.3f;  // hvor mye som svinger

  float flicker = minLevel + flickerAmt * (noise / 255.0f);
  if (flicker > 1.0f) flicker = 1.0f;

  uint8_t r = (uint8_t)(baseR * flicker);
  uint8_t g = (uint8_t)(baseG * flicker);
  uint8_t b = (uint8_t)(baseB * flicker);

  return strip.Color(r, g, b);
}

// Basisfarge (uten blafring) kun rød → mørk oransje (ingen gult)
// t i sekunder
void baseColorForTime(float t, uint8_t &r, uint8_t &g, uint8_t &b) {
  const uint8_t darkRedR     = 120;
  const uint8_t darkRedG     = 0;
  const uint8_t darkRedB     = 0;

  const uint8_t darkOrangeR  = 255;
  const uint8_t darkOrangeG  = 80;  // oransje, men ikke gul
  const uint8_t darkOrangeB  = 0;

  if (t < fadeToOrangeStart) {
    // frem til 4.5s: mørk rød
    r = darkRedR; g = darkRedG; b = darkRedB;
  } else if (t < fadeToOrangeEnd) {
    // 4.5–13.5s: mørk rød -> mørk oransje, via lys rød
    float phase = (t - fadeToOrangeStart) / (fadeToOrangeEnd - fadeToOrangeStart); // 0..1

    uint8_t midR = 220, midG = 20, midB = 0; // varm rød, lite grønn
    if (phase < 0.5f) {
      // mørk rød -> lys rød
      float u = phase / 0.5f;
      r = (uint8_t)(darkRedR + u * (midR - darkRedR));
      g = (uint8_t)(darkRedG + u * (midG - darkRedG));
      b = (uint8_t)(darkRedB + u * (midB - darkRedB));
    } else {
      // lys rød -> mørk oransje
      float u = (phase - 0.5f) / 0.5f;
      r = (uint8_t)(midR + u * (darkOrangeR - midR));
      g = (uint8_t)(midG + u * (darkOrangeG - midG));
      b = (uint8_t)(midB + u * (darkOrangeB - midB));
    }
  } else if (t < allOffTime) {
    // 13.5–40s: hold mørk oransje
    r = darkOrangeR; g = darkOrangeG; b = darkOrangeB;
  } else {
    r = g = b = 0;
  }
}

// ================== SETUP ==================
void setup() {
  M5.begin(true, false, true);

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();

  pinMode(TTL_PIN, OUTPUT);
  digitalWrite(TTL_PIN, LOW);

  clearStrip();
}

// ================== LOOP ===================
void loop() {
  M5.update();

  if (M5.Btn.wasPressed()) {
    if (mode == OFF || mode == DONE) {
      startMillis = millis();
      lastFrameMs = startMillis;
      mode = PLAYING;
      clearStrip();
      digitalWrite(TTL_PIN, LOW);
    } else if (mode == PLAYING) {
      mode = OFF;
      clearStrip();
      digitalWrite(TTL_PIN, LOW);
    }
  }

  uint32_t now = millis();
  if ((now - lastFrameMs) < FRAME_MS) return;
  lastFrameMs = now;

  if (mode == PLAYING) {
    float t = (now - startMillis) / 1000.0f;

    // 40s: alt av
    if (t >= allOffTime) {
      clearStrip();
      mode = DONE;
      return;
    }

    // global basisfarge (rød->oransje)
    uint8_t baseR, baseG, baseB;
    baseColorForTime(t, baseR, baseG, baseB);

    for (uint8_t ring = 0; ring < RINGS; ring++) {
      uint8_t ledsInRing = getLedsForRing(ring);
      float ringPos = (float)ring / (float)(RINGS - 1); // 0=bunn,1=topp

      for (uint8_t y = 0; y < ledsInRing; y++) {
        uint32_t color = 0;
        float ledPos = ringPos;

        uint16_t idx = XY(ring, y);
        if (idx == UINT16_MAX || idx >= USED_LEDS) continue;

        if (t < fillDuration) {
          // 0–3s: gradvis fyll til mørk rød, med sterk blafring og litt av/på
          float fillPhase = t / fillDuration; // 0..1
          if (ledPos <= fillPhase) {
            bool on = (random(0, 100) < 85); // 85% sannsynlighet for på
            if (on) {
              color = lavaFlicker(120, 0, 0, ledPos, now, true, idx);
            } else {
              color = 0;
            }
          } else {
            color = 0;
          }
        } else {
          // Etter 3s: alltid på, kun farge/blafring endres (lava)
          bool strong = true; // mer blafring hele tiden etter 3s
          color = lavaFlicker(baseR, baseG, baseB, ledPos, now, strong, idx);
        }

        strip.setPixelColor(idx, color);
      }
    }

    strip.show();
  }
}
