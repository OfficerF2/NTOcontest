#include <FastLED.h>

#define LED_PIN     15
#define LED_PIN2  14
#define NUM_LEDS    3
#define BRIGHTNESS  50

CRGB leds[NUM_LEDS];

void setup() {
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  FastLED.addLeds<NEOPIXEL, LED_PIN2>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
}

void loop() {

  leds[0] = CRGB::Red;
  leds[1] = CRGB::Black;
  leds[2] = CRGB::Black; 
  FastLED.show();
  delay(15000); 

  leds[0] = CRGB::Black;
  leds[1] = CRGB::Yellow;
  leds[2] = CRGB::Black;
  FastLED.show();
  delay(2000);


  leds[0] = CRGB::Black;
  leds[1] = CRGB::Black;
  leds[2] = CRGB::Green;
  FastLED.show();
  delay(15000);

  leds[0] = CRGB::Black;
  leds[1] = CRGB::Yellow;
  leds[2] = CRGB::Black;
  FastLED.show();
  delay(2000);
}
