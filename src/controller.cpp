#include "state.h"
#include "settings.h"
#include "tuya.h"
#include "datalog.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

State state;

static const uint32_t SENSOR_STALE_MS = 5UL * 60 * 1000;   // fail safe: off if no reading for 5 min
static const uint32_t RETRY_MS        = 30UL * 1000;       // first retry after a failed command
static const uint32_t RETRY_MAX_MS    = 30UL * 60 * 1000;  // backoff cap (webhook quotas are small)
static const uint32_t REASSERT_MS     = 10UL * 60 * 1000;  // Tuya: re-send state periodically
static const uint32_t TEST_HOLD_MS    = 60UL * 1000;       // manual test: leave the plug alone this long

static bool     desiredOn = false;
static uint32_t lastAttemptMs = 0;
static uint32_t lastOkMs = 0;
static uint8_t  failCount = 0;
static bool     testFailed = false;

bool inHeatingWindow(time_t now) {
  struct tm tm; localtime_r(&now, &tm);
  if (settings.weekendAllDay && (tm.tm_wday == 0 || tm.tm_wday == 6)) return true;
  int m = tm.tm_hour * 60 + tm.tm_min;
  int s = settings.nightStart, e = settings.nightEnd;
  if (s == e) return false;
  return s > e ? (m >= s || m < e) : (m >= s && m < e);
}

static bool webhook(const String& url, String& err) {
  if (url.isEmpty()) { err = "webhook URL not set"; return false; }
  HTTPClient http;
  WiFiClientSecure secure;
  WiFiClient plain;
  bool ok;
  if (url.startsWith("https")) { secure.setInsecure(); ok = http.begin(secure, url); }
  else ok = http.begin(plain, url);
  if (!ok) { err = "bad URL"; return false; }
  http.setTimeout(8000);
  int code = http.GET();
  String body = code > 0 ? http.getString() : String();
  http.end();
  if (code >= 200 && code < 300) return true;
  err = code > 0 ? "HTTP " + String(code) : String("connection failed (") + HTTPClient::errorToString(code) + ")";
  body.trim();
  if (body.length()) err += " " + body.substring(0, 120);  // e.g. {"error":"DEVICE_NOT_FOUND"}
  return false;
}

bool heaterSend(bool on, const char* reason) {
  if (WiFi.status() != WL_CONNECTED) { state.lastCmdResult = "WiFi not connected"; return false; }
  String err;
  bool ok = false;
  switch (settings.heaterMode) {
    case HEATER_TUYA:
      ok = tuya::setSwitch(settings.tuyaIp, settings.tuyaId, settings.tuyaKey, settings.tuyaVer, settings.tuyaDp, on, err);
      break;
    case HEATER_WEBHOOK:
      ok = webhook(on ? settings.hookOn : settings.hookOff, err);
      break;
    default:
      err = "no heater backend configured";
  }
  time_t now = time(nullptr);
  state.lastCmdTime = now;
  if (ok) {
    bool changed = !state.heaterKnown || state.heaterOn != on;
    state.heaterOn = on;
    state.heaterKnown = true;
    state.lastCmdResult = String("OK: ") + (on ? "ON" : "OFF");
    if (changed) state.heaterChangedAt = now;
    if (changed) datalog::event(now, String("Heater ") + (on ? "ON" : "OFF") + " (" + reason + ")");
  } else {
    state.lastCmdResult = String("FAILED ") + (on ? "ON" : "OFF") + ": " + err;
    datalog::event(now, state.lastCmdResult + " (" + reason + ")");
  }
  return ok;
}

// Without the hold, the next tick (within 2 s) would switch the plug straight back to
// what the controller wants, so a test would barely be visible (and cost a webhook call).
bool heaterTest(bool on) {
  bool ok = heaterSend(on, "manual test");
  if (settings.heaterMode == HEATER_NONE) return ok;  // nothing to hold
  testFailed = !ok;
  state.testHoldUntilMs = millis() + TEST_HOLD_MS;
  if (state.testHoldUntilMs == 0) state.testHoldUntilMs = 1;
  return ok;
}

void controllerKick() { lastAttemptMs = 0; failCount = 0; }

void boost(uint16_t minutes) {
  state.testHoldUntilMs = 0;  // boost takes over from a test
  if (minutes == 0) {
    state.boostUntilMs = 0;
    datalog::event(time(nullptr), "Boost cancelled");
  } else {
    state.boostUntilMs = millis() + minutes * 60000UL;
    if (state.boostUntilMs == 0) state.boostUntilMs = 1;
    datalog::event(time(nullptr), "Boost " + String(minutes) + " min");
  }
  lastAttemptMs = 0;  // act immediately
}

void controllerTick() {
  uint32_t ms = millis();
  time_t now = time(nullptr);
  state.window = state.timeValid && inHeatingWindow(now);

  bool sensorOk = state.lastGoodReadMs != 0 && (ms - state.lastGoodReadMs) < SENSOR_STALE_MS && !isnan(state.temp);
  bool boosting = state.boostUntilMs != 0 && (int32_t)(state.boostUntilMs - ms) > 0;
  if (state.boostUntilMs != 0 && !boosting) { state.boostUntilMs = 0; datalog::event(now, "Boost ended"); }

  bool want;
  const char* reason;
  if (boosting && !sensorOk) {
    // Never heat blind: a dead sensor ends the boost like it blocks normal heating.
    want = false; reason = "sensor failure";
    state.boostUntilMs = 0;
    datalog::event(now, "Boost cancelled (sensor failure)");
  } else if (boosting) {
    want = state.temp < settings.offTemp;
    reason = want ? "boost" : "boost, reached off temp";
    if (!want) state.boostUntilMs = 0;
  } else if (!settings.enabled) {
    want = false; reason = "disabled";
  } else if (!state.timeValid) {
    want = false; reason = "time not synced";
  } else if (!state.window) {
    want = false; reason = "outside schedule";
  } else if (!sensorOk) {
    want = false; reason = "sensor failure";
  } else {
    bool currentlyOn = state.heaterKnown ? state.heaterOn : false;
    if (state.temp <= settings.onTemp) { want = true; reason = "below on temp"; }
    else if (state.temp >= settings.offTemp) { want = false; reason = "above off temp"; }
    else { want = currentlyOn; reason = "hysteresis"; }
  }

  state.reason = reason;
  state.wantOn = want;
  if (state.testHoldUntilMs) {
    if (settings.heaterMode != HEATER_NONE && (int32_t)(state.testHoldUntilMs - ms) > 0) return;
    state.testHoldUntilMs = 0;
    datalog::event(now, "Test finished, automatic control resumed");
    // A failed test may still have switched the plug (e.g. a webhook that ran but timed
    // out), so forget what we think it is and send the wanted state once more.
    if (testFailed) state.heaterKnown = false;
    testFailed = false;
    lastAttemptMs = 0; failCount = 0;  // converge immediately
  }
  if (settings.heaterMode == HEATER_NONE) return;

  if (want != desiredOn) { desiredOn = want; lastAttemptMs = 0; failCount = 0; }
  bool pending = !state.heaterKnown || state.heaterOn != desiredOn;
  // Tuya is local and cheap: periodically re-assert so a plug toggled by hand (or
  // one that missed a packet) converges. Webhooks only fire on change/retry.
  if (!pending && settings.heaterMode == HEATER_TUYA && ms - lastOkMs > REASSERT_MS) pending = true;

  // While WiFi is down, don't try (or grow the backoff): send as soon as it returns.
  if (WiFi.status() != WL_CONNECTED) { lastAttemptMs = 0; failCount = 0; return; }

  // Exponential backoff on failure: 30s, 1m, 2m, 4m ... capped at 30 min, so a bad
  // webhook config can't burn through a monthly quota (Voice Monkey free = 200/month).
  uint32_t retryMs = failCount ? min(RETRY_MAX_MS, RETRY_MS << min<uint8_t>(failCount - 1, 6)) : 0;
  if (pending && (lastAttemptMs == 0 || ms - lastAttemptMs >= retryMs)) {
    lastAttemptMs = ms;
    if (heaterSend(desiredOn, reason)) { lastOkMs = ms; failCount = 0; }
    else if (failCount < 255) failCount++;
  }
}
