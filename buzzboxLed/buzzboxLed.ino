#include <FastLED.h>
#include <map>

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// CONFIG //
// MODE config
#define MODE_BUTTON_PIN 4
#define MODE_COUNT 10 // can't be initialized automatically, as MODES can only be initialized after setup (for respective functions to be available)
#define MODE_STRIP_DATA_PIN 27

// DIMMER config
#define DIM_POTI_PIN 32

// DOUBLE-STRIP config
#define D_STRIP_NUM_LEDS 72
#define D_STRIP_DATA_PIN 25
#define FRAMES_PER_SECOND 1000c

// BOTTOM-STRIP config
#define B_STRIP_NUM_LEDS 101 // different strip led counts have to differ, as they're used for identification
#define B_STRIP_DATA_PIN 26
#define B_STRIP_OFFSET 5

// STROBE config
#define STROBE_BUTTON_PIN 15
#define STROBE_MODE 5

// AUDIO config
#define AUDIO_PIN 33

// STATE vars
CLEDController *controllers[3]; // controller references: 0-mode, 1-double_strip, 2-bottom_strip
CRGB modeStripLeds[MODE_COUNT];
CRGB dStripLeds[D_STRIP_NUM_LEDS];
CRGB bStripLeds[B_STRIP_NUM_LEDS];
unsigned int curMode = -1; // synced between strips (id is equal, explicit mode may be different - see mode definitions per strip)
int stripBrightness = 50; // 0-255 | also synced
bool modeChangeOccurred = false;
int audioRaw = 0;
double audioRawMin = 0;
int audioRawMax = 0;
double audioScaled = 0;
double audioScaledMax = 0;
double audioScaledSens = 0;

// FUNCTIONS //
// BASE functions
void setup() {
  delay(3000); // boot recovery, and a moment of silence
  Serial.begin(9600);

  // MODE
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  controllers[0] = &FastLED.addLeds<WS2812B, MODE_STRIP_DATA_PIN, GRB>(modeStripLeds, MODE_COUNT)
  .setDither(true)
  .setCorrection(TypicalLEDStrip); // https://forum.arduino.cc/t/fastled-what-does-setcorrection-typicalledstrip-do/621342/3

  // DIMMER
  pinMode(DIM_POTI_PIN, INPUT); // not necessary, but explicitly clear

  // D-STRIP
  controllers[1] = &FastLED.addLeds<WS2812B, D_STRIP_DATA_PIN, GRB>(dStripLeds, D_STRIP_NUM_LEDS)
  .setDither(true)
  .setCorrection(TypicalLEDStrip);

  // B-STRIP
  controllers[2] = &FastLED.addLeds<WS2812B, B_STRIP_DATA_PIN, GRB>(bStripLeds, B_STRIP_NUM_LEDS)
  .setDither(true)
  .setCorrection(TypicalLEDStrip);

  // INIT LEDs
  FastLED.clear();
  setMode(0); // IMPORTANT, as default mode is -1 (to trigger initial LED turn on)
  showLeds();
  
  // STROBE
  pinMode(STROBE_BUTTON_PIN, INPUT_PULLUP);

  // AUDIO
  pinMode(AUDIO_PIN, INPUT); // not necessary, but explicitly clear
}

// modes list has to be initialized after setup, for functions to be present in scope
typedef void (*SimplePatternList[])(CRGB*, int);
SimplePatternList D_MODES = {rainbowLoop, cylonLoop, prideLoop, sinelonLoop, visualizerLoop, strobeLoop, fireLoop, bpmLoop, cycleLoop, redBlueLoop};
SimplePatternList B_MODES = {rainbowLoop, cylonLoop, prideLoop, sinelonLoop, visualizerLoop, strobeLoop, fireBottomLoop, bpmLoop, cycleLoop, redBlueLoop};

void loop() {
  // update LED arrays
  D_MODES[curMode](dStripLeds, D_STRIP_NUM_LEDS);
  B_MODES[curMode](bStripLeds, B_STRIP_NUM_LEDS);
  displayMode();
  // show updated arrays on strips
  showLeds();

  // reset here to enable state resetting for both strips (aka don't reset before current mode function for both strips has been executed)
  modeChangeOccurred = false;
  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND / 2); // divide per amount of strips
}

void showLeds() {
  controllers[0]->showLeds(255);
  controllers[1]->showLeds(stripBrightness);
  controllers[2]->showLeds(stripBrightness);
}

bool nonBlockingTasks() {
  checkModeButton();
  checkDimPoti();
  checkStrobeButton();
  audioRaw = analogRead(AUDIO_PIN)*4;

  // keep low signal at a constant level
  if(audioRaw < 2600) {
    audioRaw = 0;
  } else {
    // adapt audio signal max & sensitivity
    audioRawMax -= 5;
    if(audioRaw > audioRawMax) {
      audioRawMax += 100;
    } 
  }
  
  audioRawMin = audioRawMax*0.9; // factor defines lower bound of scaled tube
  // set scaled values for loops
  audioScaledMax = audioRawMax-audioRawMin;
  audioScaledSens = audioScaledMax*0.90; // factor = threshold for scaled
  audioScaled = audioRaw*audioScaledMax/audioRawMax;

  if(audioScaled > audioScaledSens) {
    Serial.print("Audio-In: ");
    Serial.print(audioScaled);
    Serial.print("/");
    Serial.print(audioScaledMax);
    Serial.print(" ");
    Serial.print(audioScaled*100/audioScaledMax);
    Serial.println("%");
  }

  // return whether to interrupt mode for pending mode change
  if(modeChangeOccurred) {
    FastLED.clear();
    // modeChangeOccurred = false; // moved to loop to enable state resetting for both strips
    return true;
  } else {
    return false;
  }
}


// MODE functions
int lastModeButtonState = HIGH;
int lastCleanModeButtonState = HIGH;
unsigned long lastModeButtonDebounceTime = 0;
unsigned long debounceDelay = 50;
void checkModeButton() {
  int state = digitalRead(MODE_BUTTON_PIN);

  // If the switch changed, due to noise or pressing:
  if (state != lastModeButtonState) {
    lastModeButtonDebounceTime = millis();
  }

  if ((millis() - lastModeButtonDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (state != lastCleanModeButtonState) {
      lastCleanModeButtonState = state;

      if (lastCleanModeButtonState == LOW) {
        int nextMode = (curMode + 1) % MODE_COUNT;
        // skip strobe
        if(nextMode == STROBE_MODE) {
          nextMode = (nextMode + 1) % MODE_COUNT;
        }
        setMode(nextMode);
        Serial.print("New mode: ");
        Serial.println(nextMode);
      }
    }
  }

  lastModeButtonState = state;
}

void setMode(unsigned int newMode) {
  if(newMode == curMode) {
    return;
  }

  curMode = newMode;
  
  Serial.print("Switched to mode ");
  Serial.println(newMode);

  modeChangeOccurred = true;
}

void displayMode() {
  for (int idx = 0; idx < MODE_COUNT; idx++) {
    modeStripLeds[idx] = CRGB::Black;
  }
  modeStripLeds[curMode] = CRGB::White;
}

// DIMMER functions
void checkDimPoti() {
  int val = analogRead(DIM_POTI_PIN);
  val = map(val, 0, 4095, 0, 255);
  stripBrightness = val;
}

// STROBE functions
int lastStrobeButtonState = HIGH;
int lastCleanStrobeButtonState = HIGH;
unsigned long lastStrobeButtonDebounceTime = 0;
unsigned int strobePrevMode = 0;
void checkStrobeButton() {
  int state = digitalRead(STROBE_BUTTON_PIN);

  // If the switch changed, due to noise or pressing:
  if (state != lastStrobeButtonState) {
    lastStrobeButtonDebounceTime = millis();
  }

  if ((millis() - lastStrobeButtonDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (state != lastCleanStrobeButtonState) {
      lastCleanStrobeButtonState = state;

      if (lastCleanStrobeButtonState == LOW) {
        // activate strobe mode
        strobePrevMode = curMode;
        setMode(STROBE_MODE);
        Serial.println("Strobe: activated");
      } else {
        // return to previous mode
        setMode(strobePrevMode);
        Serial.println("Strobe: deactivated");
      }
    }
  }

  lastStrobeButtonState = state;
}


// STRIP functions
// CONVENTIONS:
// name main function "<smth>Loop"
// add params "CRGB *leds, int numLeds"
// call "nonBlockingTasks"-func somewhere where it's regularly called (e.g. in computationally intensive loops)

// CYCLE //
int cycleMinPause = 50;
// state maps for different strips
std::map<int, int> cycleLastTime = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}};
std::map<int, int> cycleState = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}};

void cycleLoop(CRGB *leds, int numLeds) {
  int state = cycleState[numLeds];
  int lastTime  = cycleLastTime[numLeds];
  
  if(audioScaled > audioScaledSens && millis() - lastTime > cycleMinPause) { // advance, if signal threshold is met and min pause has passed
    lastTime = millis();
    state = (state + 1) % 6;
  } else if(millis() - lastTime > cycleMinPause*3) { // skip black during silent parts (during 3x min pause no changes)
    if(state % 2 == 1) {
      state = (state + 1) % 6;
    }
  }
  
  CRGB color = CRGB::Black;
  // 1, 3, 5 are black
  if (state == 0) {
    color = CRGB::Red;
  } else if (state == 2) {
    color = CRGB::Green;
  } else if (state == 4) {
    color = CRGB::Blue;
  }
  
  setLed(leds, numLeds, 0, numLeds, color);

  // update state maps
  cycleLastTime[numLeds] = lastTime;
  cycleState[numLeds] = state;

  // run other periodic tasks, that require non-blocking execution
  if(nonBlockingTasks()) {
    cycleLastTime[numLeds] = 0;
    cycleState[numLeds] = 0;
    return;
  }
}
// CYCLE END //

// REDBLUE //
int rbMinPause = 150;
std::map<int, int> rbOffset = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 5}};
// state maps
std::map<int, bool> rbState = {{D_STRIP_NUM_LEDS, false}, {B_STRIP_NUM_LEDS, false}};
std::map<int, int> rbLastTime = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}};

void redBlueLoop(CRGB *leds, int numLeds) {
  bool state = rbState[numLeds];
  int lastTime = rbLastTime[numLeds];
  
  if(audioScaled > audioScaledSens && millis() - lastTime > rbMinPause) {
    lastTime = millis();
    state = !state;
    redBlueSet(leds, numLeds, state);
  }

  EVERY_N_MILLISECONDS(200) {
    state = !state;
    redBlueSet(leds, numLeds, state);
  }

  // update state maps
  rbState[numLeds] = state;
  rbLastTime[numLeds] = lastTime;

  // run other periodic tasks, that require non-blocking execution
  if(nonBlockingTasks()) {
    rbState[numLeds] = false;
    rbLastTime[numLeds] = 0;
    return;
  }
}

void redBlueSet(CRGB *leds, int numLeds, int state) {
  int startOffset = rbOffset[numLeds];
  
  int startIdx = 0 + startOffset;
  int midIdx = numLeds/2 + startOffset;
  int endIdx = numLeds + startOffset;
  CRGB color1 = CRGB::Red;
  CRGB color2 = CRGB::Blue;
  if(!state) { // swap direction
    color1 = CRGB::Blue;
    color2 = CRGB::Red;
  }
  setLed(leds, numLeds, startIdx, midIdx, color1);
  setLed(leds, numLeds, midIdx, endIdx, color2);
}
// REDBLUE END //

// STROBE //
// state maps
std::map<int, int> strobeState = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}};
void strobeLoop(CRGB *leds, int numLeds) {
  int state = strobeState[numLeds];
  EVERY_N_MILLISECONDS(5) {
    state = (state + 1) % 8;

    if (state == 0) {
      setLed(leds, numLeds, 0, numLeds, CRGB::White);
    } else {
      setLed(leds, numLeds, 0, numLeds, CRGB::Black);
    }
  }

  // update state maps
  strobeState[numLeds] = state;

  // run other periodic tasks, that require non-blocking execution
  if(nonBlockingTasks()) {
    strobeState[numLeds] = 0;
    return;
  }
}
// STROBE END //

// PRIDE2015 //
// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.

int prideMinPause = 50;
// state maps
std::map<int, uint16_t> pridePseudotime = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}};
std::map<int, uint16_t> prideLastMillis = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}};
std::map<int, uint16_t> prideHue16 = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}};
std::map<int, int> prideLastTime = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}};
std::map<int, bool> prideSpeedup = {{D_STRIP_NUM_LEDS, false}, {B_STRIP_NUM_LEDS, false}};

void prideLoop(CRGB *leds, int numLeds)
{
  uint16_t pseudotime = pridePseudotime[numLeds];
  uint16_t lastMillis = prideLastMillis[numLeds];
  uint16_t hue16 = prideHue16[numLeds]; //gHue * 256;
  int lastTime = prideLastTime[numLeds];
  bool speedup = prideSpeedup[numLeds];

  // reset temporary speedup
  if(millis() - lastTime > prideMinPause) {
    speedup = false;
  }

  // temporary speedup during audio peaks
  if(audioScaled > audioScaledSens && millis() - lastTime > prideMinPause) {
    lastTime = millis();
    speedup = true;
  }

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - lastMillis;
  deltams *= 5; // own speedup factor
  deltams *= speedup ? 5 : 1; // audio peak factor
  lastMillis  = ms;
  pseudotime += deltams * msmultiplier;
  hue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = pseudotime;

  for ( uint16_t i = 0 ; i < numLeds; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV( hue8, sat8, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (numLeds - 1) - pixelnumber;

    nblend(leds[pixelnumber], newcolor, 64);
  }

  // update state maps
  pridePseudotime[numLeds] = pseudotime;
  prideLastMillis[numLeds] = lastMillis;
  prideHue16[numLeds] = hue16;
  prideLastTime[numLeds] = lastTime;
  prideSpeedup[numLeds] = speedup;

  // run other periodic tasks, that require non-blocking execution
  if(nonBlockingTasks()) {
    pridePseudotime[numLeds] = 0;
    prideLastMillis[numLeds] = 0;
    prideHue16[numLeds] = 0;
    prideLastTime[numLeds] = 0;
    prideSpeedup[numLeds] = false;
    return;
  }
}
// PRIDE2015 END //

// CYLON //
int cylonMinPause = 50;
std::map<int, int> cylonOffset = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 5}};
// state maps
std::map<int, bool> cylonForward = {{D_STRIP_NUM_LEDS, true}, {B_STRIP_NUM_LEDS, true}};
std::map<int, int> cylonIdx = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}};
std::map<int, uint8_t> cylonHue = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}};
std::map<int, int> cylonLastTime = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}};
std::map<int, bool> cylonSpeedup = {{D_STRIP_NUM_LEDS, false}, {B_STRIP_NUM_LEDS, false}};

void cylonLoop(CRGB *leds, int numLeds) {
  int startOffset = cylonOffset[numLeds];
  bool forward = cylonForward[numLeds];
  int idx = cylonIdx[numLeds];
  uint8_t hue = cylonHue[numLeds];
  int lastTime = cylonLastTime[numLeds];
  bool speedup = cylonSpeedup[numLeds];

  // reset temporary speedup
  if(millis() - lastTime > cylonMinPause) {
    speedup = false;
  }

  // temporary speedup during audio peaks
  if(audioScaled > audioScaledSens && millis() - lastTime > cylonMinPause) {
    lastTime = millis();
    speedup = true;
  }

  if(speedup) {
    Serial.println("Speedup");
  }

  // check whether direction has to be switched
  if(forward && idx >= numLeds) {
    forward = false;
    idx = numLeds-1;
  } else if(!forward && idx < 0) {
    forward = true;
    idx = 0;
  }

  int realIdx = (idx+startOffset) % numLeds;
  // First slide the led in one direction
  if(forward) {
    // Set the i'th led to red (adapted to offset)
    leds[realIdx] = CHSV(hue++, 255, 255);
    // Show the leds
    // FastLED.show(); // commenting this out might require everything that alters the LED array after this point to be moved to the method start
    // now that we've shown the leds, reset the i'th led to black
    // leds[i] = CRGB::Black;
    cylonFadeall(leds, numLeds);
    
    // Wait a little bit before we loop around and do it again
    FastLED.delay(speedup ? 0 : 4);

    idx++;
  } else { // Now go in the other direction.
    // Set the i'th led to red (adapted to offset)
    leds[realIdx] = CHSV(hue++, 255, 255);
    // Show the leds
    // FastLED.show(); // commenting this out might require everything that alters the LED array after this point to be moved to the method start
    // now that we've shown the leds, reset the i'th led to black
    // leds[i] = CRGB::Black;
    cylonFadeall(leds, numLeds);

    idx--;
  }

  // update state maps
  cylonForward[numLeds] = forward;
  cylonIdx[numLeds] = idx;
  cylonHue[numLeds] = hue;
  cylonLastTime[numLeds] = lastTime;
  cylonSpeedup[numLeds] = speedup;

  // run other periodic tasks, that require non-blocking execution
  if(nonBlockingTasks()) {
    cylonForward[numLeds] = true;
    cylonIdx[numLeds] = 0;
    cylonHue[numLeds] = 0;
    cylonLastTime[numLeds] = 0;
    cylonSpeedup[numLeds] = false;
    return;
  }
}

void cylonFadeall(CRGB *leds, int numLeds) {
  for (int i = 0; i < numLeds; i++) {
    leds[i].nscale8(250);
  }
}
// CYLON END //

// RAINBOW //
// state maps
std::map<int, int> rainbowHue = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}}; // rotating "base color" used by many of the patterns

void rainbowLoop(CRGB *leds, int numLeds)
{
  int hue = rainbowHue[numLeds];
  
  // do some periodic updates
  EVERY_N_MILLISECONDS( 5 ) {
    hue++;  // slowly cycle the "base color" through the rainbow
  }
  
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, numLeds, hue, 7);

  // update state maps
  rainbowHue[numLeds] = hue;

  // run other periodic tasks, that require non-blocking execution
  if(nonBlockingTasks()) {
    rainbowHue[numLeds] = 0;
    return;
  }
}
// RAINBOW END //

// SINELON //
int dotCount = 3;
int thickness = 2;
// state maps
std::map<int, CRGB> sinelonColor = {{D_STRIP_NUM_LEDS, CRGB::Purple}, {B_STRIP_NUM_LEDS, CRGB::Purple}};
std::map<int, CRGB> sinelonBgColor = {{D_STRIP_NUM_LEDS, CRGB::Blue}, {B_STRIP_NUM_LEDS, CRGB::Blue}};

void sinelonLoop(CRGB *leds, int numLeds)
{
  CRGB color = sinelonColor[numLeds];
  CRGB bgColor = sinelonBgColor[numLeds];
  
  int singleOffset = numLeds/dotCount;

  // switch color & bgColor during high audio signal
  if(audioScaled > audioScaledSens) {
    CRGB tempColor = color;
    color = bgColor;
    bgColor = tempColor;
  }
  
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, numLeds, 20);
  int basePos = beatsin16( 14, 0, numLeds - 1 );
  int invBasePos = beatsin16( 14, 0, numLeds - 1, 0, 32768);

  // calculate other positions based on basePos and strip length
  for(int dotIdx = 0; dotIdx < dotCount; dotIdx++) {
    int pos = basePos + singleOffset*dotIdx;
    for(int subIdx = 0; subIdx < thickness; subIdx++) {
      int subPos = (pos + subIdx) % numLeds;
      leds[subPos] += color;
    }
  }

  // inverted direction
  for(int dotIdx = 0; dotIdx < dotCount; dotIdx++) {
    int pos = invBasePos + singleOffset*dotIdx;
    for(int subIdx = 0; subIdx < thickness; subIdx++) {
      int subPos = (pos + subIdx) % numLeds;
      leds[subPos] += color;
    }
  }

  // replace black background with blue
  for(int idx = 0; idx < numLeds; idx++) {
    if(leds[idx] == CRGB(0, 0, 0)) {
      leds[idx] = bgColor;
    }
  }

  // update state maps
  sinelonColor[numLeds] = color;
  sinelonBgColor[numLeds] = bgColor;

  // run other periodic tasks, that require non-blocking execution
  if(nonBlockingTasks()) {
    sinelonColor[numLeds] = CRGB::Purple;
    sinelonBgColor[numLeds] = CRGB::Blue;
    return;
  }
}
// SINELON END //

// BPM //
int bpmMinPause = 50;
// state maps
std::map<int, int> bpmHue = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}};
std::map<int, int> bpmLastTime = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}};
std::map<int, bool> bpmSpeedup = {{D_STRIP_NUM_LEDS, false}, {B_STRIP_NUM_LEDS, false}};

void bpmLoop(CRGB *leds, int numLeds)
{
  int hue = bpmHue[numLeds];
  int lastTime = bpmLastTime[numLeds];
  bool speedup = bpmSpeedup[numLeds];

  // reset temporary speedup
  if(millis() - lastTime > bpmMinPause) {
    speedup = false;
  }

  // temporary speedup during audio peaks
  if(audioScaled > audioScaledSens && millis() - lastTime > bpmMinPause) {
    lastTime = millis();
    speedup = true;
  }
  
  // do some periodic updates
  EVERY_N_MILLISECONDS( speedup ? 2 : 5 ) {
    hue++;  // slowly cycle the "base color" through the rainbow
  }
  
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < numLeds; i++) { //9948
    leds[i] = ColorFromPalette(palette, hue + (i * 2), beat - hue + (i * 10));
  }

  // update state maps
  bpmHue[numLeds] = hue;
  bpmLastTime[numLeds] = lastTime;
  bpmSpeedup[numLeds] = speedup;

  // run other periodic tasks, that require non-blocking execution
  if(nonBlockingTasks()) {
    bpmHue[numLeds] = 0;
    bpmLastTime[numLeds] = 0;
    bpmSpeedup[numLeds] = false;
    return;
  }
}
// BPM END //

// FIRE2012 //
CRGBPalette16 firePal = HeatColors_p;
  // This first palette is the basic 'black body radiation' colors,
  // which run from black to red to bright yellow to white.
  // gPal = HeatColors_p;
  
  // These are other ways to set up the color palette for the 'fire'.
  // First, a gradient from black to red to yellow to white -- similar to HeatColors_p
  //   gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::Yellow, CRGB::White);

  // Second, this palette is like the heat colors, but blue/aqua instead of red/yellow
  //   gPal = CRGBPalette16( CRGB::Black, CRGB::Blue, CRGB::Aqua,  CRGB::White);

  // Third, here's a simpler, three-step gradient, from black to red to white
  //   gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::White);
bool fireReverseDirection = false;
#define FIRE_COOLING  55
#define FIRE_SPARKING 120
uint8_t fireHeat[D_STRIP_NUM_LEDS]; // temperature readings at each simulation cell

// made for double-strips only (see fireHeat array)
void fireLoop(CRGB *leds, int numLeds)
{
  // Step 1.  Cool down every cell a little
  for ( int i = 0; i < numLeds; i++) {
    fireHeat[i] = qsub8( fireHeat[i],  random8(0, ((FIRE_COOLING * 10) / numLeds) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for ( int k = numLeds - 1; k >= 2; k--) {
    fireHeat[k] = (fireHeat[k - 1] + fireHeat[k - 2] + fireHeat[k - 2] ) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if ( random8() < FIRE_SPARKING ) {
    int y = random8(7);
    fireHeat[y] = qadd8( fireHeat[y], random8(160, 255) );
  }

  // Step 4.  Map from heat cells to LED colors
  for ( int j = 0; j < numLeds; j++) {
    // Scale the heat value from 0-255 down to 0-240
    // for best results with color palettes.
    uint8_t colorindex = scale8( fireHeat[j], 240);
    CRGB color = ColorFromPalette( firePal, colorindex);
    int pixelnumber;
    if ( fireReverseDirection ) {
      pixelnumber = (numLeds - 1) - j;
    } else {
      pixelnumber = j;
    }
    leds[pixelnumber] = color;
  }

  // run other periodic tasks, that require non-blocking execution
  if(nonBlockingTasks()) {
    return;
  }
}

void fireBottomLoop(CRGB *leds, int numLeds) {
  setLed(leds, numLeds, 0, numLeds, dStripLeds[62]);
}
// FIRE2012 END //

// VISUALIZER //
int visDecay = 0; // how many ms before one light decay
int visWheelSpeed = 3;
// state maps
std::map<int, int> visWheelPos = {{D_STRIP_NUM_LEDS, 255}, {B_STRIP_NUM_LEDS, 255}};
std::map<int, int> visDecayCheck = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}};
std::map<int, long> visReact = {{D_STRIP_NUM_LEDS, 0}, {B_STRIP_NUM_LEDS, 0}}; // number of LEDs being lit

void visualizerLoop(CRGB *leds, int numLeds)
{
  int wheelPos = visWheelPos[numLeds];
  int decayCheck = visDecayCheck[numLeds];
  long react = visReact[numLeds];
  
  int audio_input = audioScaled; // add x2 here for more sensitivity

  if (audio_input > 0)
  {
    long pre_react = map(audio_input, 0, audioScaledMax, 0, numLeds); // translate audio level to number of LEDs

    if (pre_react > react) // only adjust level of LED if level higher than current level
      react = pre_react;
      Serial.print(pre_react);
      Serial.print("->");
      Serial.println(react);
  }

  visualizerRainbow(leds, numLeds, react, wheelPos); // apply color

  wheelPos = wheelPos - visWheelSpeed; // speed of color wheel
  if (wheelPos < 0) // reset color wheel
    wheelPos = 255;

  // remove LEDs
  decayCheck++;
  if (decayCheck > visDecay)
  {
    decayCheck = 0;
    if (react > 0)
      react--;
  }

  // update state maps
  visWheelPos[numLeds] = wheelPos;
  visDecayCheck[numLeds] = decayCheck;
  visReact[numLeds] = react;

  // run other periodic tasks, that require non-blocking execution
  if(nonBlockingTasks()) {
    return;
  }
}

// function to generate color based on virtual wheel
// https://github.com/NeverPlayLegit/Rainbow-Fader-FastLED/blob/master/rainbow.ino
CRGB Scroll(int pos) {
  CRGB color (0, 0, 0);
  if (pos < 85) {
    color.g = 0;
    color.r = ((float)pos / 85.0f) * 255.0f;
    color.b = 255 - color.r;
  } else if (pos < 170) {
    color.g = ((float)(pos - 85) / 85.0f) * 255.0f;
    color.r = 255 - color.g;
    color.b = 0;
  } else if (pos < 256) {
    color.b = ((float)(pos - 170) / 85.0f) * 255.0f;
    color.g = 255 - color.b;
    color.r = 1;
  }
  return color;
}

// function to get and set color
// the original function went backwards (now waves out from first LED)
// https://github.com/NeverPlayLegit/Rainbow-Fader-FastLED/blob/master/rainbow.ino
int bottomOffset = 5; // bottom visualizer starts with offset and is mirrored vertically & horizontally
int bottomQuarterCount = B_STRIP_NUM_LEDS/4;
void visualizerRainbow(CRGB *leds, int numLeds, int react, int wheelPos)
{
  if(numLeds == D_STRIP_NUM_LEDS) {
    for (int i = numLeds - 1; i >= 0; i--) {
      if (i < react)
        leds[i] = Scroll((i * 256 / 50 + wheelPos) % 256);
      else
        leds[i] = CRGB(0, 0, 0);
    }
  } else if(numLeds == B_STRIP_NUM_LEDS) {
    react /= 4;
    // start to first quarter
    for (int i = bottomOffset + bottomQuarterCount - 1; i >= bottomOffset; i--) {
      if (i < react + bottomOffset)
        leds[i] = Scroll((i * 256 / 50 + wheelPos) % 256);
      else
        leds[i] = CRGB(0, 0, 0);
    }
    // half to first quarter
    for (int i = bottomOffset + bottomQuarterCount; i < bottomOffset + 2*bottomQuarterCount; i++) {
      if (i > bottomOffset + 2*bottomQuarterCount - react)
        leds[i] = Scroll((i * 256 / 50 + wheelPos) % 256);
      else
        leds[i] = CRGB(0, 0, 0);
    }
    // half to third quarter
    for (int i = bottomOffset + 3*bottomQuarterCount - 1; i >= bottomOffset + 2*bottomQuarterCount; i--) {
      if (i < react + bottomOffset + 2*bottomQuarterCount)
        leds[i] = Scroll((i * 256 / 50 + wheelPos) % 256);
      else
        leds[i] = CRGB(0, 0, 0);
    }
    // end to third quarter
    for (int i = bottomOffset + 3*bottomQuarterCount; i < bottomOffset + 4*bottomQuarterCount; i++) {
      if (i > bottomOffset + 4*bottomQuarterCount - react)
        leds[i%numLeds] = Scroll((i * 256 / 50 + wheelPos) % 256);
      else
        leds[i%numLeds] = CRGB(0, 0, 0);
    }
  }
}
// VISUALIZER END //

// UTIL functions
void setLed(CRGB *leds, int numLeds, int startIdx, int endIdx, const struct CRGB &color) {
  if(endIdx <= numLeds) {
    for (int idx = startIdx; idx < endIdx; idx++) {
      leds[idx] = color;
    }
  } else {
    for (int idx = startIdx; idx < numLeds; idx++) {
      leds[idx] = color;
    }
    for(int idx = 0; idx < endIdx-numLeds; idx++) {
      leds[idx] = color;
    }
  }
}
