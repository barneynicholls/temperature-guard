#pragma once
#include <Arduino.h>

struct State {
  float    temp = NAN;          // averaged, calibrated (°C)
  float    hum = NAN;
  float    rawTemp = NAN;
  uint32_t lastGoodReadMs = 0;
  uint32_t sensorErrors = 0;
  bool     timeValid = false;
  bool     window = false;        // inside allowed heating window
  bool     heaterOn = false;      // what we last commanded successfully
  bool     heaterKnown = false;   // false until first command succeeds
  uint32_t boostUntilMs = 0;      // 0 = no boost
  uint32_t testHoldUntilMs = 0;   // after a manual test, automatic switching pauses until this
  String   lastCmdResult;
  time_t   lastCmdTime = 0;
  time_t   heaterChangedAt = 0;
  String   reason;              // why the controller wants the current state
  bool     wantOn = false;
};

extern State state;

// One recursive lock around shared state/SD/settings: held by loop() while it
// works and by web handlers (which run on the httpd task). Implemented in main.cpp.
void appLock();
void appUnlock();
struct AppGuard { AppGuard() { appLock(); } ~AppGuard() { appUnlock(); } };

// Heater actions (implemented in controller.cpp)
bool heaterSend(bool on, const char* reason);
bool heaterTest(bool on);  // manual Test on/off: sends, then holds automatic control briefly
void controllerTick();
void controllerKick();  // forget retry backoff, e.g. after settings change
bool inHeatingWindow(time_t now);
void boost(uint16_t minutes);
