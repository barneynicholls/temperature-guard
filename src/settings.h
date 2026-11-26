#pragma once
#include <Arduino.h>

// Defaults for a fresh device. Override at build time, e.g. in platformio.ini:
//   build_flags = -DDEFAULT_TITLE='"Garage"' -DDEFAULT_HOSTNAME='"garage-temp"'
// Leave WiFi empty to configure it from the setup access point on first boot.
#ifndef DEFAULT_TITLE
#define DEFAULT_TITLE "Temperature Guard"
#endif
#ifndef DEFAULT_HOSTNAME
#define DEFAULT_HOSTNAME "temp-guard"
#endif
#ifndef DEFAULT_WIFI_SSID
#define DEFAULT_WIFI_SSID ""
#endif
#ifndef DEFAULT_WIFI_PASS
#define DEFAULT_WIFI_PASS ""
#endif

enum HeaterMode : uint8_t { HEATER_NONE = 0, HEATER_TUYA = 1, HEATER_WEBHOOK = 2 };

struct Settings {
  // Control
  bool    enabled        = true;
  float   onTemp         = 11.0f;   // heater on at or below this (°C)
  float   offTemp        = 15.0f;   // heater off at or above this (°C)
  float   tempOffset     = 0.0f;    // calibration added to raw sensor reading
  uint16_t nightStart    = 22 * 60; // minutes after midnight
  uint16_t nightEnd      = 7 * 60;
  bool    weekendAllDay  = true;    // Sat + Sun allowed all day

  // Logging
  uint16_t logIntervalS  = 60;
  uint16_t retentionDays = 90;

  // Status light: 0 off, 1 low, 2 medium, 3 high
  uint8_t ledLevel       = 1;

  // Heater backend
  uint8_t heaterMode     = HEATER_NONE;
  String  tuyaId;
  String  tuyaKey;
  String  tuyaIp;
  String  tuyaVer        = "3.3";
  uint8_t tuyaDp         = 1;
  String  hookOn;
  String  hookOff;

  // Network / access
  String  title          = DEFAULT_TITLE;     // shown in the web UI
  String  wifiSsid       = DEFAULT_WIFI_SSID;
  String  wifiPass       = DEFAULT_WIFI_PASS;
  String  hostname       = DEFAULT_HOSTNAME;  // <hostname>.local
  String  webPass;                  // empty = no login
};

extern Settings settings;

void settingsLoad();
void settingsSave();
void settingsReset();
