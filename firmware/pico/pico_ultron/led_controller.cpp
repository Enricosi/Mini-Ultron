#include <Adafruit_NeoPixel.h>
#include "led_controller.h"

// GPIO16 / 8 LEDs confirmed against Freenove's own reference sketch
// (freenove-car-reference/.../01.6_WS2812/Freenove_4WD_Car_WS2812.h).
#define LED_PIN 16
#define LED_COUNT 8

// Low overall brightness (not just dim colors) so a full-white command could
// never be issued through this API in the first place, and so idle current
// draw stays negligible on the shared 5V rail.
#define LED_BRIGHTNESS 20  // out of 255

struct LedColor {
  uint8_t r, g, b;
};

// One fixed, dim color per state - visible but deliberately not full-color.
// Order must match enum LedState.
static const LedColor kColors[] = {
  {0, 0, 0},     // LED_OFF
  {40, 0, 60},   // LED_IDLE - violet
  {90, 0, 0},    // LED_ANGRY - red
  {0, 70, 70},   // LED_LISTENING - cyan
  {0, 0, 90},    // LED_THINKING - blue
  {0, 90, 20},   // LED_SPEAKING - green
  {60, 60, 60},  // LED_SURPRISED - white
  {90, 30, 0},   // LED_SUSPICIOUS - orange
  {0, 0, 20},    // LED_SLEEPING - very dim blue
  {90, 0, 60},   // LED_GLITCH - magenta
};

static Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
static LedState currentState = LED_OFF;

static void showState(LedState state) {
  const LedColor &c = kColors[state];
  uint32_t color = strip.Color(c.r, c.g, c.b);
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void ledControllerInit() {
  strip.begin();
  strip.setBrightness(LED_BRIGHTNESS);
  showState(LED_OFF);      // boot state is off, matching "STOP before anything else"
  ledSetState(LED_IDLE);   // then signal ready, once init has otherwise completed
}

void ledSetState(LedState state) {
  currentState = state;
  showState(state);
}

LedState ledGetState() {
  return currentState;
}

bool ledStateForFaceName(const String &name, LedState *out) {
  if (name == "IDLE") *out = LED_IDLE;
  else if (name == "ANGRY") *out = LED_ANGRY;
  else if (name == "LISTENING") *out = LED_LISTENING;
  else if (name == "THINKING") *out = LED_THINKING;
  else if (name == "SPEAKING") *out = LED_SPEAKING;
  else if (name == "SURPRISED") *out = LED_SURPRISED;
  else if (name == "SUSPICIOUS") *out = LED_SUSPICIOUS;
  else if (name == "SLEEPING") *out = LED_SLEEPING;
  else if (name == "GLITCH") *out = LED_GLITCH;
  else return false;
  return true;
}
