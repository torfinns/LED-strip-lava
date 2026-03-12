#include <M5Atom.h>
#include <Adafruit_NeoPixel.h>

#define PIN        22
#define NUM_PIXELS 10

Adafruit_NeoPixel strip(NUM_PIXELS, PIN, NEO_GRB + NEO_KHZ800);

int mode = 0;  // 0=blå, 1=rød, 2=grønn, 3=hvit

uint32_t currentColor() {
  switch (mode) {
    case 0: return strip.Color(0,   0,   255); // Blå
    case 1: return strip.Color(255, 0,   0  ); // Rød
    case 2: return strip.Color(0,   255, 0  ); // Grønn
    case 3: return strip.Color(255, 255, 255); // Hvit
  }
  return strip.Color(0, 0, 255);
}

void setup() {
  M5.begin(true, false, true);
  strip.begin();
  strip.setBrightness(50);
  strip.show();
}

void loop() {
  M5.update();

  // Hvis knappen trykkes, bytt farge
  if (M5.Btn.wasPressed()) {
    mode = (mode + 1) % 4;  // 0→1→2→3→0...
  }

  uint32_t c = currentColor();

  for (int i = 0; i < NUM_PIXELS; i++) {
    strip.clear();
    strip.setPixelColor(i, c);
    strip.show();
    delay(50);
  }
}
