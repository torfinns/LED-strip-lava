#include <M5Atom.h>
#include <Adafruit_NeoPixel.h>

// =====================================================
//  KONFIG: LED, TID, FARGER, TTL
// =====================================================

// ---------- LED / NeoPixel ----------
#define PIN        22          // Data-pin til LED-strip
#define NUM_PIXELS 263
#define BRIGHTNESS 200         // Global lysstyrke (0–255)

static_assert(NUM_PIXELS >= 263, "NUM_PIXELS er for lavt");

Adafruit_NeoPixel strip(NUM_PIXELS, PIN, NEO_GRB + NEO_KHZ800);

// ---------- TTL ----------
#define TTL_OUT_PIN 19         // TTL ut til neste modul
#define TTL_IN_PIN  21         // TTL inn fra forrige modul

// Lengde på TTL-pulser ut (i ms)
const uint32_t TTL_START_PULSE_MS   = 500;   // start-signal videre
const uint32_t TTL_STOP_PULSE_MS    = 2000;  // stopp-signal videre

// Anti-prell for input (TTL + knapp), i ms
const uint32_t DEBOUNCE_MS          = 500;   // 500 ms sperre etter hendelse

// Terskel for å skille PÅ/AV på TTL IN og knapp (puls/trykklengde)
const uint32_t ON_MAX_MS            = 1000;  // < 1000 ms = START, >= 1000 ms = STOP

// Tid (sek) etter sekvensstart når vi sender TTL “START”-puls ut
const float    TTL_START_PULSE_SEC  = 5.5f;

// Tid (sek) etter sekvensstart når vi sender TTL “STOP seq”-puls ut
const float    TTL_STOPSEQ_SEC      = 120.0f;

// ---------- Sylinder-geometri ----------
const uint8_t  RINGS     = 17;
const uint16_t USED_LEDS = 263;

static_assert(USED_LEDS <= NUM_PIXELS, "USED_LEDS overstiger NUM_PIXELS");

uint8_t getLedsForRing(uint8_t ring) {
  return (ring < 8) ? 16 : 15;
}

uint16_t XY(uint8_t ring, uint8_t y) {
  if (ring >= RINGS) return UINT16_MAX;
  uint8_t ledsThisRing = getLedsForRing(ring);
  if (y >= ledsThisRing) return UINT16_MAX;

  uint16_t base = 0;
  for (uint8_t r = 0; r < ring; r++) base += getLedsForRing(r);

  return (ring % 2 == 0)
    ? base + y
    : (uint16_t)(base + (ledsThisRing - 1 - y));
}

// ---------- Timing / FPS ----------
#define FRAMES_PER_SECOND 60
#define FRAME_MS (1000 / FRAMES_PER_SECOND)

// ---------- ANIMASJONSTID ----------
const float FILL_DURATION_SEC        = 3.0f;
const float FADE_TO_ORANGE_START_SEC = 4.5f;
const float FADE_TO_ORANGE_END_SEC   = 13.5f;
const float ALL_OFF_TIME_SEC         = 300.0f;

// ---------- FARGER ----------
const uint8_t DARKRED_R    = 120, DARKRED_G = 0,   DARKRED_B = 0;
const uint8_t DARKORANGE_R = 255, DARKORANGE_G = 110, DARKORANGE_B = 0;
const uint8_t MIDRED_R     = 220, MIDRED_G = 20,  MIDRED_B = 0;

// =====================================================
//  STATE
// =====================================================

enum Mode { OFF, PLAYING, DONE };
Mode mode = OFF;

uint32_t startMillis       = 0;
uint32_t lastFrameMs       = 0;

// Knapp (samme logikk som TTL inn: kort = start, lang = stopp)
bool     buttonPressed     = false;
uint32_t buttonPressStart  = 0;
uint32_t lastButtonEventMs = 0;

// TTL OUT
bool     ttlStartPulseActive    = false;
uint32_t ttlStartPulseStartMs   = 0;
bool     ttlStopSeqPulseActive  = false;
uint32_t ttlStopSeqPulseStartMs = 0;
bool     ttlStopSeqTriggered    = false;

// ---------- TTL IN via interrupt ----------
volatile bool     ttlISRState        = false;   // siste nivå sett i ISR
volatile uint32_t ttlISREventTimeMs  = 0;       // tidspunkt for siste edge
volatile bool     ttlISRNewEdge      = false;   // flagg: ny edge oppdaget

bool     ttlMeasuredHigh        = false;
uint32_t ttlHighStartMs         = 0;
uint32_t lastTtlHandledEventMs  = 0;

// =====================================================
//  HJELPEFUNKSJONER
// =====================================================

void clearStrip() {
  for (uint16_t i = 0; i < USED_LEDS; i++) strip.setPixelColor(i, 0);
  strip.show();
}

uint32_t lavaFlicker(uint8_t baseR, uint8_t baseG, uint8_t baseB,
                     float posNorm, uint32_t timeMs, bool strongFlicker,
                     uint16_t ledIndex)
{
  uint32_t seed = (uint32_t)ledIndex * 1103515245u
                + (uint32_t)(posNorm * 1000.0f) * 12345u
                + timeMs / 5;

  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  uint8_t noise = (uint8_t)(seed & 0xFF);

  float minLevel   = strongFlicker ? 0.4f : 0.6f;
  float flickerAmt = strongFlicker ? 0.4f : 0.3f;

  float flicker = minLevel + flickerAmt * (noise / 255.0f);
  if (flicker > 1.0f) flicker = 1.0f;

  uint8_t r = (uint8_t)(baseR * flicker);
  uint8_t g = (uint8_t)(baseG * flicker);
  uint8_t b = (uint8_t)(baseB * flicker);

  return strip.Color(r, g, b);
}

void baseColorForTime(float t, uint8_t &r, uint8_t &g, uint8_t &b) {
  if (t < FADE_TO_ORANGE_START_SEC) {
    r = DARKRED_R; g = DARKRED_G; b = DARKRED_B;
  } else if (t < FADE_TO_ORANGE_END_SEC) {
    float phase = (t - FADE_TO_ORANGE_START_SEC) /
                  (FADE_TO_ORANGE_END_SEC - FADE_TO_ORANGE_START_SEC);

    if (phase < 0.5f) {
      float u = phase / 0.5f;
      r = (uint8_t)(DARKRED_R + u * (MIDRED_R - DARKRED_R));
      g = (uint8_t)(DARKRED_G + u * (MIDRED_G - DARKRED_G));
      b = (uint8_t)(DARKRED_B + u * (MIDRED_B - DARKRED_B));
    } else {
      float u = (phase - 0.5f) / 0.5f;
      r = (uint8_t)(MIDRED_R + u * (DARKORANGE_R - MIDRED_R));
      g = (uint8_t)(MIDRED_G + u * (DARKORANGE_G - MIDRED_G));
      b = (uint8_t)(MIDRED_B + u * (DARKORANGE_B - MIDRED_B));
    }
  } else if (t < ALL_OFF_TIME_SEC) {
    r = DARKORANGE_R; g = DARKORANGE_G; b = DARKORANGE_B;
  } else {
    r = g = b = 0;
  }
}

void startSequence() {
  startMillis        = millis();
  lastFrameMs        = startMillis;
  mode               = PLAYING;
  clearStrip();

  ttlStartPulseActive    = false;
  ttlStopSeqPulseActive  = false;
  ttlStopSeqTriggered    = false;
  digitalWrite(TTL_OUT_PIN, LOW);
}

void stopSequence() {
  mode = OFF;
  clearStrip();
  ttlStartPulseActive    = false;
  ttlStopSeqPulseActive  = false;
  digitalWrite(TTL_OUT_PIN, LOW);
}

// ---------- ISR for TTL IN ----------
void IRAM_ATTR ttlISR() {
  ttlISRState       = digitalRead(TTL_IN_PIN);
  ttlISREventTimeMs = millis();
  ttlISRNewEdge     = true;
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
  digitalWrite(TTL_OUT_PIN, LOW);

  pinMode(TTL_IN_PIN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(TTL_IN_PIN), ttlISR, CHANGE);

  clearStrip();
}

// =====================================================
//  LOOP
// =====================================================

void loop() {
  M5.update();
  uint32_t now = millis();

  // --- Knapp på Atom Lite: kort trykk = START, langt = STOP (som TTL) ---
  bool btnNow = M5.Btn.isPressed();

  if (btnNow && !buttonPressed) {
    // rising på knappen
    if (now - lastButtonEventMs >= DEBOUNCE_MS) {
      buttonPressed    = true;
      buttonPressStart = now;
    }
  }

  if (btnNow && buttonPressed) {
    // langt trykk -> STOP så snart vi passerer 1000 ms
    uint32_t dur = now - buttonPressStart;
    if (dur >= ON_MAX_MS) {
      lastButtonEventMs = now;
      buttonPressed     = false;
      if (mode == PLAYING) stopSequence();
    }
  }

  if (!btnNow && buttonPressed) {
    // slipp: kort trykk -> START
    uint32_t dur = now - buttonPressStart;
    lastButtonEventMs = now;
    buttonPressed = false;
    if (dur < ON_MAX_MS) {
      if (mode == OFF || mode == DONE) startSequence();
    }
  }

  // --- Håndter TTL IN basert på ISR-data (som knappen) ---
  bool     edge;
  bool     level;
  uint32_t edgeTime;
  noInterrupts();
  edge     = ttlISRNewEdge;
  level    = ttlISRState;
  edgeTime = ttlISREventTimeMs;
  ttlISRNewEdge = false;
  interrupts();

  if (edge) {
    if (level) {
      // rising: start måling
      if (now - lastTtlHandledEventMs >= DEBOUNCE_MS) {
        ttlMeasuredHigh = true;
        ttlHighStartMs  = edgeTime;
      }
    } else {
      // falling: kort puls -> START
      if (ttlMeasuredHigh) {
        uint32_t width = edgeTime - ttlHighStartMs;
        lastTtlHandledEventMs = now;
        ttlMeasuredHigh = false;

        if (width < ON_MAX_MS) {
          if (mode == OFF || mode == DONE) startSequence();
        }
      }
    }
  }

  if (ttlMeasuredHigh) {
    // Lang puls -> STOP straks vi passerer 1000 ms
    uint32_t widthNow = now - ttlHighStartMs;
    if (widthNow >= ON_MAX_MS) {
      lastTtlHandledEventMs = now;
      ttlMeasuredHigh = false;
      if (mode == PLAYING) stopSequence();
    }
  }

  // --- FPS-begrensning ---
  if ((now - lastFrameMs) < FRAME_MS) return;
  lastFrameMs = now;

  if (mode == PLAYING) {
    float t = (now - startMillis) / 1000.0f;

    // TTL OUT: 500ms START-puls ved 5.5s
    if (!ttlStartPulseActive && t >= TTL_START_PULSE_SEC) {
      ttlStartPulseActive   = true;
      ttlStartPulseStartMs  = now;
      digitalWrite(TTL_OUT_PIN, HIGH);
    }
    if (ttlStartPulseActive && (now - ttlStartPulseStartMs >= TTL_START_PULSE_MS)) {
      ttlStartPulseActive = false;
      digitalWrite(TTL_OUT_PIN, LOW);
    }

    // TTL OUT: 2000ms STOP-seq-puls ved 120s
    if (!ttlStopSeqTriggered && t >= TTL_STOPSEQ_SEC) {
      ttlStopSeqTriggered    = true;
      ttlStopSeqPulseActive  = true;
      ttlStopSeqPulseStartMs = now;
      digitalWrite(TTL_OUT_PIN, HIGH);
    }
    if (ttlStopSeqPulseActive && (now - ttlStopSeqPulseStartMs >= TTL_STOP_PULSE_MS)) {
      ttlStopSeqPulseActive = false;
      digitalWrite(TTL_OUT_PIN, LOW);
    }

    // Alt av etter ALL_OFF_TIME_SEC
    if (t >= ALL_OFF_TIME_SEC) {
      clearStrip();
      mode = DONE;
      ttlStartPulseActive   = false;
      ttlStopSeqPulseActive = false;
      digitalWrite(TTL_OUT_PIN, LOW);
      return;
    }

    // Farge / lava-animasjon
    uint8_t baseR, baseG, baseB;
    baseColorForTime(t, baseR, baseG, baseB);

    for (uint8_t ring = 0; ring < RINGS; ring++) {
      uint8_t ledsInRing = getLedsForRing(ring);
      float ringPos = (float)ring / (float)(RINGS - 1);

      for (uint8_t y = 0; y < ledsInRing; y++) {
        uint32_t color = 0;
        float ledPos = ringPos;

        uint16_t idx = XY(ring, y);
        if (idx == UINT16_MAX || idx >= USED_LEDS) continue;

        if (t < FILL_DURATION_SEC) {
          float fillPhase = t / FILL_DURATION_SEC;
          if (ledPos <= fillPhase) {
            bool on = (random(0, 100) < 85);
            if (on) color = lavaFlicker(DARKRED_R, DARKRED_G, DARKRED_B,
                                        ledPos, now, true, idx);
            else    color = 0;
          } else {
            color = 0;
          }
        } else {
          bool strong = true;
          color = lavaFlicker(baseR, baseG, baseB, ledPos, now, strong, idx);
        }

        strip.setPixelColor(idx, color);
      }
    }

    strip.show();
  }
}
