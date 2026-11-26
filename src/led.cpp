#include "led.h"
#include "settings.h"
#include "state.h"
#include <WiFi.h>

// LED data pin. The board this was built on has the NeoPixel data on
// GPIO 21 with no separate power pin. On a stock Adafruit Feather ESP32-S3 use
//   -DSTATUS_LED_PIN=33 -DSTATUS_LED_POWER_PIN=21
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN 21
#endif

namespace led {

struct Rgb { uint8_t r, g, b; };
static const Rgb PURPLE = {150, 0, 255}, RED = {255, 0, 0}, YELLOW = {255, 150, 0}, ORANGE = {255, 55, 0},
                 GREEN = {0, 255, 30}, BLUE = {0, 70, 255}, WHITE = {255, 200, 160};

static const uint32_t PULSE_MS = 700;
static uint32_t pulseAt = 0;
static bool pulseOk = true;
static uint32_t lastMs = 0;
static Rgb shown = {1, 1, 1};

void begin() {
#ifdef STATUS_LED_POWER_PIN
  pinMode(STATUS_LED_POWER_PIN, OUTPUT);
  digitalWrite(STATUS_LED_POWER_PIN, HIGH);
#endif
  neopixelWrite(STATUS_LED_PIN, 20, 20, 20);  // dim white while booting
}

void pulse(bool readOk) { pulseAt = millis(); pulseOk = readOk; if (!pulseAt) pulseAt = 1; }

static Rgb stateColour() {
  bool sensorOk = state.lastGoodReadMs && millis() - state.lastGoodReadMs < 60000;
  bool cmdFailed = settings.heaterMode != HEATER_NONE && state.lastCmdResult.startsWith("FAILED");
  if (WiFi.status() != WL_CONNECTED) return PURPLE;
  if (!sensorOk || cmdFailed) return RED;
  if (!state.timeValid) return YELLOW;
  if (state.heaterKnown && state.heaterOn) return ORANGE;
  if (!settings.enabled) return WHITE;
  return state.window ? GREEN : BLUE;
}

void tick() {
  uint32_t ms = millis();
  if (ms - lastMs < 20) return;
  lastMs = ms;

  // Base and peak brightness per level (0 off, 1 low, 2 medium, 3 high).
  static const float BASE[] = {0, 0.025f, 0.07f, 0.18f};
  static const float PEAK[] = {0, 0.30f, 0.55f, 1.00f};
  uint8_t lvl = settings.ledLevel > 3 ? 3 : settings.ledLevel;

  Rgb c = stateColour();
  float k = BASE[lvl];
  if (pulseAt && lvl) {
    uint32_t dt = ms - pulseAt;
    if (dt < PULSE_MS) {
      float x = (float)dt / PULSE_MS;          // quick rise, smooth fall
      float env = x < 0.15f ? x / 0.15f : 1 - (x - 0.15f) / 0.85f;
      env *= env;
      k += (PEAK[lvl] - k) * env;
      if (!pulseOk) c = RED;                     // a failed reading flashes red
    } else {
      pulseAt = 0;
    }
  }
  Rgb o = {(uint8_t)(c.r * k + 0.5f), (uint8_t)(c.g * k + 0.5f), (uint8_t)(c.b * k + 0.5f)};
  if (o.r != shown.r || o.g != shown.g || o.b != shown.b) {
    neopixelWrite(STATUS_LED_PIN, o.r, o.g, o.b);
    shown = o;
  }
}

}  // namespace led
