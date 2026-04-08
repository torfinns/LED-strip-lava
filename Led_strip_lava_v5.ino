#include <M5Atom.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

// =====================================================
//  KONFIG: LED, TID, FARGER, TTL
// =====================================================

#define PIN        22
#define NUM_PIXELS 263
#define BRIGHTNESS 200

Adafruit_NeoPixel strip(NUM_PIXELS, PIN, NEO_GRB + NEO_KHZ800);

#define TTL_OUT_PIN 19
#define TTL_IN_PIN  21

const uint8_t  RINGS     = 17;
const uint16_t USED_LEDS = 263;

const float FILL_DURATION_SEC  = 10.0f;
const float FULL_FLAME_END_SEC = 16.0f;
const float FADE_END_SEC       = 30.0f;
const float BLEND_DURATION     = 6.0f;
const float ALL_OFF_TIME_SEC   = 300.0f;
const float FADE_OUT_TIME_SEC  = 5.0f;

const uint32_t TTL_DEBOUNCE_MS     = 50;
const uint32_t BTN_DEBOUNCE_MS     = 500;
const uint32_t ON_MAX_MS           = 1000;
const float    TTL_START_PULSE_SEC = FILL_DURATION_SEC + 1.0f;

// =====================================================
//  GEOMETRI
// =====================================================

uint8_t getLedsForRing(uint8_t ring) {
  return (ring < 8) ? 16 : 15;
}

uint16_t ringBase(uint8_t ring) {
  uint16_t base = 0;
  for (uint8_t r = 0; r < ring; r++) base += getLedsForRing(r);
  return base;
}

uint16_t FLAME_XY(uint8_t ring, uint8_t col) {
  if (ring >= RINGS) return UINT16_MAX;
  if (col >= getLedsForRing(ring)) return UINT16_MAX;
  uint16_t idx = ringBase(ring) + col;
  return (idx < USED_LEDS) ? idx : UINT16_MAX;
}

#define FRAMES_PER_SECOND 60
#define FRAME_MS          (1000 / FRAMES_PER_SECOND)

const uint8_t DARKRED_R    = 120, DARKRED_G  =  0, DARKRED_B  = 0;
const uint8_t MIDRED_R     = 220, MIDRED_G   = 20, MIDRED_B   = 0;
const uint8_t DARKORANGE_R = 255, DARKORANGE_G = 85, DARKORANGE_B = 0;

#define COOLING  40
#define SPARKING 120

// =====================================================
//  STATE
// =====================================================

enum Mode { OFF, PLAYING, FADING, DONE };
Mode mode = OFF;

uint32_t startMillis = 0, lastFrameMs = 0;
uint32_t fadeStartMs = 0;

// Knapp
bool     buttonPressed     = false;
uint32_t buttonPressStart  = 0;
uint32_t lastButtonEventMs = 0;

// TTL OUT
bool ttlOutHigh = false;

// TTL IN — ISR (first-edge-wins)
volatile bool ttlISRState   = false;
volatile bool ttlISRNewEdge = false;

// TTL IN — loop
uint32_t lastTtlHandledEventMs = 0;
bool     ttlStopPending        = false;

// Animasjon
const uint8_t FIRE_COLS = 15;
uint8_t heat[FIRE_COLS][RINGS];
int8_t  colOffset[FIRE_COLS];
uint8_t colRotation[FIRE_COLS];

// =====================================================
//  HJELPEFUNKSJONER
// =====================================================

void clearStrip() {
  for (uint16_t i = 0; i < USED_LEDS; i++) strip.setPixelColor(i, 0);
  strip.show();
}

void clearHeat() {
  for (uint8_t c = 0; c < FIRE_COLS; c++)
    for (uint8_t r = 0; r < RINGS; r++)
      heat[c][r] = 0;
}

void randomizeOffsets() {
  for (uint8_t c = 0; c < FIRE_COLS; c++) {
    colOffset[c]   = (int8_t)random(-3, 4);
    colRotation[c] = (uint8_t)random(0, 15);
  }
}

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

float smoothstep(float a, float b, float x) {
  float t = clamp01((x - a) / (b - a));
  return t * t * (3.0f - 2.0f * t);
}

uint8_t lerpU8(uint8_t a, uint8_t b, float t) {
  return (uint8_t)(a + (b - a) * clamp01(t) + 0.5f);
}

void baseColorForTime(float t, uint8_t &r, uint8_t &g, uint8_t &b) {
  if (t < FULL_FLAME_END_SEC) {
    r = DARKRED_R; g = DARKRED_G; b = DARKRED_B;
  } else if (t < FADE_END_SEC) {
    float s = smoothstep(FULL_FLAME_END_SEC, FADE_END_SEC, t);
    if (s < 0.5f) {
      float u = smoothstep(0.0f, 0.5f, s);
      r = lerpU8(DARKRED_R, MIDRED_R, u);
      g = lerpU8(DARKRED_G, MIDRED_G, u);
      b = lerpU8(DARKRED_B, MIDRED_B, u);
    } else {
      float u = smoothstep(0.5f, 1.0f, s);
      r = lerpU8(MIDRED_R, DARKORANGE_R, u);
      g = lerpU8(MIDRED_G, DARKORANGE_G, u);
      b = lerpU8(MIDRED_B, DARKORANGE_B, u);
    }
  } else {
    r = DARKORANGE_R; g = DARKORANGE_G; b = DARKORANGE_B;
  }
}

uint32_t heatToColor(uint8_t temperature, uint8_t baseR, uint8_t baseG, uint8_t baseB) {
  float f = temperature / 255.0f;
  uint8_t r, g, b;
  if (f < 0.4f) {
    float u = f / 0.4f;
    r = (uint8_t)(baseR * 0.35f * u);
    g = (uint8_t)(baseG * 0.10f * u);
    b = 0;
  } else {
    float u = (f - 0.4f) / 0.6f;
    r = lerpU8((uint8_t)(baseR * 0.35f), baseR, u);
    g = lerpU8((uint8_t)(baseG * 0.10f), baseG, u);
    b = lerpU8(0, baseB, u);
  }
  return strip.Color(r, g, b);
}

uint32_t lavaFlicker(uint8_t baseR, uint8_t baseG, uint8_t baseB,
                     float posNorm, uint32_t timeMs, uint16_t ledIndex) {
  uint32_t seed = (uint32_t)ledIndex * 1103515245u
                + (uint32_t)(posNorm * 1000.0f) * 12345u
                + timeMs / 5;
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  float n = (seed & 0xFF) / 255.0f;
  float f = clamp01(0.4f + 0.5f * n);
  return strip.Color((uint8_t)(baseR * f),
                     (uint8_t)(baseG * f),
                     (uint8_t)(baseB * f));
}

void fire2012Column(uint8_t col, uint8_t sparking, uint8_t cooling) {
  for (uint8_t i = 0; i < RINGS; i++)
    heat[col][i] = qsub8(heat[col][i], random8(0, ((cooling * 10) / RINGS) + 2));
  for (int k = RINGS - 1; k >= 2; k--)
    heat[col][k] = (heat[col][k - 1] + heat[col][k - 2] + heat[col][k - 2]) / 3;
  if (random8() < sparking) {
    uint8_t y = random8(0, 4);
    heat[col][y] = qadd8(heat[col][y], random8(160, 255));
  }
}

// =====================================================
//  SEKVENS-KONTROLL
// =====================================================

void setTtlOut(bool high) {
  ttlOutHigh = high;
  digitalWrite(TTL_OUT_PIN, high ? HIGH : LOW);
}

void startSequence() {
  startMillis = millis();
  lastFrameMs = startMillis;
  mode        = PLAYING;
  clearStrip();
  clearHeat();
  randomizeOffsets();
  setTtlOut(false);
  // ttlStopPending nullstilles ikke her —
  // FALLING som kom rett før START skal fortsatt vinne
}

void stopSequence() {
  if (mode != PLAYING) return;
  if (ttlOutHigh) setTtlOut(false);  // FALLING kun hvis linja var HIGH
  fadeStartMs = millis();
  mode        = FADING;
}

// =====================================================
//  ISR — first-edge-wins
// =====================================================

void IRAM_ATTR ttlISR() {
  if (ttlISRNewEdge) return;  // forrige flanke ikke prosessert — ignorer
  ttlISRState   = digitalRead(TTL_IN_PIN);
  ttlISRNewEdge = true;
}

// =====================================================
//  SETUP
// =====================================================

void setup() {
  M5.begin(true, false, true);
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();

  pinMode(TTL_OUT_PIN, OUTPUT);
  setTtlOut(false);

  // INPUT — ingen intern pull, ekstern spenningsdeler styrer nivået
  pinMode(TTL_IN_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(TTL_IN_PIN), ttlISR, CHANGE);

  randomSeed(analogRead(0) ^ micros());
  clearStrip();
}

// =====================================================
//  LOOP
// =====================================================

void loop() {
  M5.update();
  uint32_t now = millis();

  // -----------------------------------------------
  // KNAPP: kort trykk = START, langt = STOPP
  // -----------------------------------------------
  bool btnNow = M5.Btn.isPressed();

  if (btnNow && !buttonPressed) {
    if (now - lastButtonEventMs >= BTN_DEBOUNCE_MS) {
      buttonPressed    = true;
      buttonPressStart = now;
    }
  }

  if (btnNow && buttonPressed) {
    if (now - buttonPressStart >= ON_MAX_MS) {
      lastButtonEventMs = now;
      buttonPressed     = false;
      if (mode == PLAYING) stopSequence();
    }
  }

  if (!btnNow && buttonPressed) {
    lastButtonEventMs = now;
    buttonPressed     = false;
    if (now - buttonPressStart < ON_MAX_MS) {
      if (mode == OFF || mode == DONE) {
        ttlStopPending = false;  // manuell start overstyrer pending STOPP
        startSequence();
      }
    }
  }

  // -----------------------------------------------
  // TTL IN: RISING = START, FALLING = STOPP
  // -----------------------------------------------
  bool edge;
  bool level;
  noInterrupts();
  edge          = ttlISRNewEdge;
  level         = ttlISRState;
  ttlISRNewEdge = false;
  interrupts();

  if (edge) {
    if (now - lastTtlHandledEventMs >= TTL_DEBOUNCE_MS) {
      lastTtlHandledEventMs = now;
      if (level) {
        // RISING = START
        ttlStopPending = false;  // RISING overstyrer pending STOPP
        if (mode == OFF || mode == DONE) startSequence();
      } else {
        // FALLING = STOPP — lagres uansett mode
        ttlStopPending = true;
      }
    }
  }

  // Trigger STOPP så snart vi er i PLAYING
  if (ttlStopPending && mode == PLAYING) {
    ttlStopPending = false;
    stopSequence();
  }

  // -----------------------------------------------
  // FPS-begrensning
  // -----------------------------------------------
  if ((now - lastFrameMs) < FRAME_MS) return;
  lastFrameMs = now;

  if (mode != PLAYING && mode != FADING) return;

  // -----------------------------------------------
  // FADING
  // -----------------------------------------------
  float fadeScale = 1.0f;
  if (mode == FADING) {
    float p = (now - fadeStartMs) / (FADE_OUT_TIME_SEC * 1000.0f);
    if (p > 1.0f) p = 1.0f;
    fadeScale = 1.0f - smoothstep(0.0f, 1.0f, p);
    if (p >= 1.0f) {
      clearStrip();
      strip.setBrightness(BRIGHTNESS);
      setTtlOut(false);       // linja tilbake til LOW
      ttlStopPending = false; // ren state til neste runde
      mode = DONE;
      return;
    }
  }

  float t = (now - startMillis) / 1000.0f;

  // -----------------------------------------------
  // TTL OUT: RISING til neste modul ved t=11s
  // Linja forblir HIGH — FALLING sendes av stopSequence()
  // -----------------------------------------------
  if (mode == PLAYING) {
    if (!ttlOutHigh && t >= TTL_START_PULSE_SEC) {
      setTtlOut(true);  // RISING → neste modul starter
    }
    if (t >= ALL_OFF_TIME_SEC) {
      stopSequence();
      return;
    }
  }

  // -----------------------------------------------
  // ANIMASJON
  // -----------------------------------------------
  uint8_t baseR, baseG, baseB;
  baseColorForTime(t, baseR, baseG, baseB);

  uint8_t currentSparking, currentCooling, globalMaxRing;
  bool    useFillMask;
  float   glowBlend;

  if (t < FILL_DURATION_SEC) {
    float p         = t / FILL_DURATION_SEC;
    globalMaxRing   = (uint8_t)(p * (RINGS - 1) + 0.5f);
    currentCooling  = (uint8_t)(70 - 30 * p);
    currentSparking = (uint8_t)(80 + 80 * p);
    useFillMask     = true;
    glowBlend       = 0.0f;
  } else if (t < FULL_FLAME_END_SEC) {
    globalMaxRing   = RINGS - 1;
    currentCooling  = COOLING;
    currentSparking = SPARKING;
    useFillMask     = false;
    glowBlend       = 0.0f;
  } else if (t < FADE_END_SEC) {
    globalMaxRing   = RINGS - 1;
    currentCooling  = COOLING;
    currentSparking = SPARKING;
    useFillMask     = false;
    glowBlend       = 0.0f;
  } else if (t < FADE_END_SEC + BLEND_DURATION) {
    float p         = (t - FADE_END_SEC) / BLEND_DURATION;
    globalMaxRing   = RINGS - 1;
    currentCooling  = COOLING;
    currentSparking = (uint8_t)(SPARKING * (1.0f - p));
    useFillMask     = false;
    glowBlend       = smoothstep(0.0f, 1.0f, p);
  } else {
    globalMaxRing   = RINGS - 1;
    currentCooling  = 255;
    currentSparking = 0;
    useFillMask     = false;
    glowBlend       = 1.0f;
  }

  if (glowBlend < 1.0f) {
    for (uint8_t c = 0; c < FIRE_COLS; c++)
      fire2012Column(c, currentSparking, currentCooling);
  }

  float brightnessScale = 1.0f;
  if (glowBlend >= 1.0f) {
    float g1 = 0.5f + 0.5f * sinf(now * 0.0014f);
    float g2 = 0.5f + 0.5f * sinf(now * 0.00051f + 1.3f);
    brightnessScale = 0.50f + 0.50f * (0.6f * g1 + 0.4f * g2);
  }

  brightnessScale *= fadeScale;
  strip.setBrightness((uint8_t)(BRIGHTNESS * brightnessScale));

  for (uint8_t ring = 0; ring < RINGS; ring++) {
    uint8_t ledsInRing = getLedsForRing(ring);
    float ringPos = (float)ring / (float)(RINGS - 1);

    for (uint8_t col = 0; col < ledsInRing; col++) {
      uint16_t idx = FLAME_XY(ring, col);
      if (idx == UINT16_MAX) continue;

      uint8_t timeRot = (uint8_t)((now / 800) % FIRE_COLS);
      uint8_t fc = (col + colRotation[col % FIRE_COLS] + timeRot) % FIRE_COLS;

      uint32_t color = 0;

      if (glowBlend >= 1.0f) {
        color = lavaFlicker(baseR, baseG, baseB, ringPos, now, idx);
      } else if (useFillMask) {
        int16_t colMax = (int16_t)globalMaxRing + colOffset[fc];
        if (colMax < 0) colMax = 0;
        if (colMax >= RINGS) colMax = RINGS - 1;
        color = (ring > (uint8_t)colMax)
          ? 0 : heatToColor(heat[fc][ring], baseR, baseG, baseB);
      } else if (glowBlend > 0.0f) {
        uint32_t flameColor = heatToColor(heat[fc][ring], baseR, baseG, baseB);
        uint32_t glowColor  = lavaFlicker(baseR, baseG, baseB, ringPos, now, idx);
        uint8_t fr = (flameColor >> 16) & 0xFF, fg = (flameColor >> 8) & 0xFF, fb = flameColor & 0xFF;
        uint8_t gr = (glowColor  >> 16) & 0xFF, gg = (glowColor  >> 8) & 0xFF, gb = glowColor  & 0xFF;
        color = strip.Color(lerpU8(fr, gr, glowBlend),
                            lerpU8(fg, gg, glowBlend),
                            lerpU8(fb, gb, glowBlend));
      } else {
        color = heatToColor(heat[fc][ring], baseR, baseG, baseB);
      }

      strip.setPixelColor(idx, color);
    }
  }

  strip.show();
}
