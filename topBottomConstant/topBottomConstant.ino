#include <FastLED.h>

// DOUBLE-STRIP config
#define D_STRIP_NUM_LEDS 72
#define D_STRIP_DATA_PIN 25

// BOTTOM-STRIP config
#define B_STRIP_NUM_LEDS 204
#define B_STRIP_DATA_PIN 26

// STATE vars
CRGB dStripLeds[D_STRIP_NUM_LEDS];
CRGB bStripLeds[B_STRIP_NUM_LEDS];

void setup() {
  // D-STRIP
  FastLED.addLeds<WS2812B, D_STRIP_DATA_PIN, GRB>(dStripLeds, D_STRIP_NUM_LEDS)
  .setDither(true)
  .setCorrection(TypicalLEDStrip);

  // B-STRIP
  FastLED.addLeds<WS2812B, B_STRIP_DATA_PIN, GRB>(bStripLeds, B_STRIP_NUM_LEDS)
  .setDither(true)
  .setCorrection(TypicalLEDStrip);
  FastLED.clear();
  FastLED.setBrightness(255);
}

void loop() {
  for(int i = 0; i < D_STRIP_NUM_LEDS; i++) {
    dStripLeds[i] = CRGB::Red;
  }
  for(int i = 0; i < B_STRIP_NUM_LEDS; i++) {
    bStripLeds[i] = CRGB::Green;
  }
  
  FastLED.show();
  delay(100);
}
