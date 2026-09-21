#include <Adafruit_NeoPixel.h>
#include "led_controller.h"

// GPIO16 / 8 LEDs confirmed against Freenove's own reference sketch
// (freenove-car-reference/.../01.6_WS2812/Freenove_4WD_Car_WS2812.h).
#define LED_PIN 16
#define LED_COUNT 8

// Low overall brightness (not just a dim color) so a full-white command
// could never be issued through this API in the first place, and so idle
// current draw stays negligible on the shared 5V rail.
#define LED_BRIGHTNESS 20  // out of 255

// Dim violet/blue - visible but deliberately not full-color, per the
// "tenue, no full-brightness/full-white" requirement.
#define LED_READY_R 40
#define LED_READY_G 0
#define LED_READY_B 60

static Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
static LedState currentState = LED_OFF;

static void showState(LedState state) {
  uint32_t color = (state == LED_READY) ? strip.Color(LED_READY_R, LED_READY_G, LED_READY_B)
                                         : strip.Color(0, 0, 0);
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void ledControllerInit() {
  strip.begin();
  strip.setBrightness(LED_BRIGHTNESS);
  showState(LED_OFF);   // boot state is off, matching "STOP before anything else"
  ledSetState(LED_READY);  // then signal ready, once init has otherwise completed
}

void ledSetState(LedState state) {
  currentState = state;
  showState(state);
}

LedState ledGetState() {
  return currentState;
}
