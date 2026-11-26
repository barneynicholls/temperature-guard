#pragma once
#include <Arduino.h>

// The Feather's on-board RGB LED as a status light: always on at a low level in a colour
// that shows the state, with a brighter pulse on every sensor reading.
//
//   purple  no WiFi (setup access point)      orange  heater on
//   red     sensor or heater-command fault    green   heating allowed, heater off
//   yellow  waiting for the clock to sync     blue    outside the schedule
//   white   frost guard turned off
namespace led {
void begin();
void pulse(bool readOk);  // call after each sensor read
void tick();              // call often from loop()
}
