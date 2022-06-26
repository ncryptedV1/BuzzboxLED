#include <FastLED.h>

#define DATA_PIN 5
#define LED_ORDER GRB
#define LED_TYPE WS2812B
#define NUM_LEDS 144

CRGB leds[NUM_LEDS];

void setup() {
  FastLED.addLeds<LED_TYPE, DATA_PIN, LED_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(255);

  FastLED.clear();
  FastLED.show();
}

void loop() {
  setColor(CRGB::Red);
  FastLED.show();
  delay(500);
  
  setColor(CRGB::Green);
  FastLED.show();
  delay(500);
  
  setColor(CRGB::Blue);
  FastLED.show();
  delay(500);
  
  setColor(CRGB::Black);
  FastLED.show();
  delay(500);
}

void setColor(const struct CRGB &color) {
  for(int idx = 0; idx < NUM_LEDS; idx++) {
    leds[idx] = color;
  }
}
