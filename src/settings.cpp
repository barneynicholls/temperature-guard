#include "settings.h"
#include <Preferences.h>

Settings settings;
static Preferences prefs;
static const char* NS = "tempguard";
static const char* OLD_NS = "studiotemp";  // pre-rename namespace, migrated once

static void loadFrom(const char* ns);

void settingsLoad() {
  // One-time migration from the old namespace so existing devices keep their settings.
  prefs.begin(NS, true);
  bool fresh = !prefs.isKey("host");
  prefs.end();
  if (fresh) {
    prefs.begin(OLD_NS, true);
    bool old = prefs.isKey("host");
    prefs.end();
    if (old) {
      loadFrom(OLD_NS);
      settingsSave();
      prefs.begin(OLD_NS, false); prefs.clear(); prefs.end();
      Serial.println("[CFG] migrated settings from old namespace");
      return;
    }
  }
  loadFrom(NS);
}

static void loadFrom(const char* ns) {
  Settings d;  // defaults
  prefs.begin(ns, true);
  settings.enabled       = prefs.getBool("enabled", d.enabled);
  settings.onTemp        = prefs.getFloat("onTemp", d.onTemp);
  settings.offTemp       = prefs.getFloat("offTemp", d.offTemp);
  settings.tempOffset    = prefs.getFloat("tOffset", d.tempOffset);
  settings.nightStart    = prefs.getUShort("nStart", d.nightStart);
  settings.nightEnd      = prefs.getUShort("nEnd", d.nightEnd);
  settings.weekendAllDay = prefs.getBool("wkend", d.weekendAllDay);
  settings.logIntervalS  = prefs.getUShort("logInt", d.logIntervalS);
  settings.retentionDays = prefs.getUShort("retDays", d.retentionDays);
  settings.heaterMode    = prefs.getUChar("hMode", d.heaterMode);
  settings.tuyaId        = prefs.getString("tId", d.tuyaId);
  settings.tuyaKey       = prefs.getString("tKey", d.tuyaKey);
  settings.tuyaIp        = prefs.getString("tIp", d.tuyaIp);
  settings.tuyaVer       = prefs.getString("tVer", d.tuyaVer);
  settings.tuyaDp        = prefs.getUChar("tDp", d.tuyaDp);
  settings.hookOn        = prefs.getString("hOn", d.hookOn);
  settings.hookOff       = prefs.getString("hOff", d.hookOff);
  settings.ledLevel      = prefs.getUChar("led", d.ledLevel);
  settings.title         = prefs.getString("title", d.title);
  settings.wifiSsid      = prefs.getString("ssid", d.wifiSsid);
  settings.wifiPass      = prefs.getString("wpass", d.wifiPass);
  settings.hostname      = prefs.getString("host", d.hostname);
  settings.webPass       = prefs.getString("webPass", d.webPass);
  prefs.end();
}

void settingsSave() {
  prefs.begin(NS, false);
  prefs.putBool("enabled", settings.enabled);
  prefs.putFloat("onTemp", settings.onTemp);
  prefs.putFloat("offTemp", settings.offTemp);
  prefs.putFloat("tOffset", settings.tempOffset);
  prefs.putUShort("nStart", settings.nightStart);
  prefs.putUShort("nEnd", settings.nightEnd);
  prefs.putBool("wkend", settings.weekendAllDay);
  prefs.putUShort("logInt", settings.logIntervalS);
  prefs.putUShort("retDays", settings.retentionDays);
  prefs.putUChar("hMode", settings.heaterMode);
  prefs.putString("tId", settings.tuyaId);
  prefs.putString("tKey", settings.tuyaKey);
  prefs.putString("tIp", settings.tuyaIp);
  prefs.putString("tVer", settings.tuyaVer);
  prefs.putUChar("tDp", settings.tuyaDp);
  prefs.putString("hOn", settings.hookOn);
  prefs.putString("hOff", settings.hookOff);
  prefs.putUChar("led", settings.ledLevel);
  prefs.putString("title", settings.title);
  prefs.putString("ssid", settings.wifiSsid);
  prefs.putString("wpass", settings.wifiPass);
  prefs.putString("host", settings.hostname);
  prefs.putString("webPass", settings.webPass);
  prefs.end();
}

void settingsReset() {
  prefs.begin(NS, false);
  prefs.clear();
  prefs.end();
  settings = Settings();
}
