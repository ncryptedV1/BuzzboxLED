#include <FastLED.h>

#define MODE_COUNT 10
#define MODE_STRIP_DATA_PIN 27
#define FRAMES_PER_SECOND 1000

CRGB modeStripLeds[MODE_COUNT];

void setup() {
  delay(3000); // boot recovery, and a moment of silence
  Serial.begin(9600);
  
  FastLED.addLeds<WS2812B, MODE_STRIP_DATA_PIN, GRB>(modeStripLeds, MODE_COUNT)
  .setDither(true)
  .setCorrection(TypicalLEDStrip);
}

void loop() {
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND / 10); // divide per amount of strips
  setLed();
}

void setLed() {
  for (int idx = 0; idx < MODE_COUNT; idx++) {
    modeStripLeds[idx] = CRGB::Black;
  }
  modeStripLeds[6] = CRGB::White;
  FastLED.show(); 
}
