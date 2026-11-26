// Temperature Guard — frost guard for a room, shed, studio or garage.
// DHT11 -> averaged temperature -> switch a Smart Life (Tuya) plug, or fire
// webhooks (e.g. Voice Monkey -> Alexa routine), with hysteresis, only during
// the allowed schedule (nightly window + optionally all weekend).

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DHT.h>
#include <esp_task_wdt.h>
#include "settings.h"
#include "state.h"
#include "datalog.h"
#include "web.h"
#include "led.h"

const char* FW_VERSION = "2.3.1";

// Pin assignments for the board as wired; change these to match yours
#define DHT_PIN      7
#define SD_SCK_PIN  12
#define SD_MISO_PIN 13
#define SD_MOSI_PIN 11
#define SD_CS_PIN   10
// Hold this button for 10 s at any time to erase all settings (factory reset). GPIO 0 is BOOT.
#ifndef RESET_BUTTON_PIN
#define RESET_BUTTON_PIN 0
#endif

static const char* TZ_UK = "GMT0BST,M3.5.0/1,M10.5.0";
static const char* AP_SSID = "TempGuard-Setup";
static const char* AP_PASS = "tempguard";

static const uint32_t SENSOR_MS = 5000;
static const int      AVG_N = 12;  // average of last 12 reads (~1 min)

static DHT dht(DHT_PIN, DHT11);
static float tBuf[AVG_N], hBuf[AVG_N];
static int bufN = 0, bufI = 0;

static uint32_t lastSensorMs = 0, lastLogMs = 0, lastPruneMs = 0, lastSdRetryMs = 0, lastCtrlMs = 0;
static uint32_t wifiDownSinceMs = 0;
static bool apOn = false;
static bool mdnsOn = false;
static SemaphoreHandle_t appMutex = nullptr;

void appLock() { if (appMutex) xSemaphoreTakeRecursive(appMutex, portMAX_DELAY); }
void appUnlock() { if (appMutex) xSemaphoreGiveRecursive(appMutex); }

static void loopBody();

static void resetButtonTick(uint32_t ms) {
  static uint32_t downSince = 0;
  if (digitalRead(RESET_BUTTON_PIN) == HIGH) { downSince = 0; return; }
  if (!downSince) { downSince = ms ? ms : 1; return; }
  if (ms - downSince < 10000) return;
  Serial.println("[CFG] Reset button held 10 s: erasing settings and restarting");
  datalog::event(time(nullptr), "Factory reset (button)");
  settingsReset();
  delay(200);
  ESP.restart();
}

void applyNetworkSettings() {
  Serial.printf("[NET] Connecting to %s\n", settings.wifiSsid.c_str());
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(apOn ? WIFI_AP_STA : WIFI_STA);
  WiFi.setHostname(settings.hostname.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
  if (mdnsOn) { MDNS.end(); mdnsOn = false; }
  wifiDownSinceMs = millis();
}

static void readSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h) || t < -40 || t > 80) {
    state.sensorErrors++;
    led::pulse(false);
    return;
  }
  led::pulse(true);
  state.rawTemp = t;
  tBuf[bufI] = t + settings.tempOffset;
  hBuf[bufI] = h;
  bufI = (bufI + 1) % AVG_N;
  if (bufN < AVG_N) bufN++;
  float ts = 0, hs = 0;
  for (int i = 0; i < bufN; i++) { ts += tBuf[i]; hs += hBuf[i]; }
  state.temp = ts / bufN;
  state.hum = hs / bufN;
  state.lastGoodReadMs = millis();
}

static void wifiTick() {
  uint32_t ms = millis();
  if (WiFi.status() == WL_CONNECTED) {
    if (wifiDownSinceMs) {
      wifiDownSinceMs = 0;
      Serial.printf("[NET] Connected, IP %s RSSI %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      datalog::event(time(nullptr), "WiFi connected " + WiFi.localIP().toString());
    }
    if (!mdnsOn && MDNS.begin(settings.hostname.c_str())) { MDNS.setInstanceName(settings.title.c_str()); MDNS.addService("https", "tcp", 443); MDNS.addService("http", "tcp", 80); mdnsOn = true; }
    if (apOn) { WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA); apOn = false; Serial.println("[NET] Setup AP off"); }
  } else {
    if (!wifiDownSinceMs) wifiDownSinceMs = ms;
    // No WiFi configured, or 2 minutes without WiFi: open a setup access point so settings stay reachable.
    if (!apOn && (settings.wifiSsid.isEmpty() || ms - wifiDownSinceMs > 120000)) {
      WiFi.mode(WIFI_AP_STA);
      // Once a web password of 8+ characters (the WPA2 minimum) is set, it is also the
      // setup AP's password, so the published default only works on an unsecured device.
      bool own = settings.webPass.length() >= 8;
      WiFi.softAP(AP_SSID, own ? settings.webPass.c_str() : AP_PASS);
      apOn = true;
      Serial.printf("[NET] Setup AP '%s' up at %s (password: %s)\n", AP_SSID, WiFi.softAPIP().toString().c_str(),
                    own ? "the web password" : AP_PASS);
    }
  }
}

void setup() {
  appMutex = xSemaphoreCreateRecursiveMutex();
  Serial.begin(115200);
  delay(1500);
  Serial.printf("\n[BOOT] Temperature Guard %s\n", FW_VERSION);

  esp_task_wdt_init(60, true);
  esp_task_wdt_add(nullptr);

  settingsLoad();
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  led::begin();
  dht.begin();
  datalog::begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  setenv("TZ", TZ_UK, 1); tzset();
  applyNetworkSettings();
  configTzTime(TZ_UK, "pool.ntp.org", "time.google.com", "time.cloudflare.com");

  webBegin();
  datalog::event(time(nullptr), String("Boot, firmware ") + FW_VERSION);
}

void loop() {
  esp_task_wdt_reset();
  loopBody();
  // Outside the app lock: stopping the server waits for handlers, which may be waiting for it.
  bool sta = WiFi.status() == WL_CONNECTED;
  webService(sta, apOn, sta ? WiFi.localIP().toString() : String());
  delay(5);  // lock released here so web handlers get a turn
}

static void loopBody() {
  AppGuard g;
  uint32_t ms = millis();

  wifiTick();
  webTick();
  resetButtonTick(ms);
  led::tick();  // reads shared state, so it runs under the lock

  if (!state.timeValid && time(nullptr) > 1700000000) {
    state.timeValid = true;
    datalog::event(time(nullptr), "Time synced");
    datalog::prune(settings.retentionDays);
    datalog::loadRecent(time(nullptr));
    lastPruneMs = ms;
  }

  if (ms - lastSensorMs >= SENSOR_MS || lastSensorMs == 0) {
    lastSensorMs = ms;
    readSensor();
  }

  if (ms - lastCtrlMs >= 2000) {
    lastCtrlMs = ms;
    controllerTick();
  }

  if (state.timeValid && !isnan(state.temp) && ms - lastLogMs >= settings.logIntervalS * 1000UL) {
    lastLogMs = ms;
    bool fresh = state.lastGoodReadMs && ms - state.lastGoodReadMs < 60000;
    if (fresh) datalog::addSample(time(nullptr), state.temp, state.hum, state.heaterKnown && state.heaterOn, state.window);
  }

  if (state.timeValid && ms - lastPruneMs > 6UL * 3600 * 1000) {
    lastPruneMs = ms;
    datalog::prune(settings.retentionDays);
  }

  if (!datalog::sdReady() && ms - lastSdRetryMs > 60000) {
    lastSdRetryMs = ms;
    if (datalog::begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN)) datalog::event(time(nullptr), "SD card mounted");
  }
}
