// HTTPS web UI + JSON API on ESP-IDF's esp_https_server (port 443), with a tiny
// plain-HTTP server on port 80 that only redirects to HTTPS.
// Handlers run on the httpd task, so everything that touches shared state takes
// the app lock (see AppGuard in state.h).

#include "web.h"
#include "web_ui.h"
#include "settings.h"
#include "state.h"
#include "datalog.h"
#include "tls_cert.h"
#include <esp_https_server.h>
#include <esp_http_server.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <SD.h>
#include <Update.h>
#include <mbedtls/base64.h>
#include <vector>

static const char* MASK = "********";
extern const char* FW_VERSION;

static httpd_handle_t https = nullptr;
static httpd_handle_t http = nullptr;
static volatile bool wantNetApply = false;
static volatile uint32_t rebootAtMs = 0;

// ---------- helpers ----------

// Each recv waits up to recv_wait_timeout (10 s); give up on a client that stalls longer.
static const int MAX_RECV_TIMEOUTS = 3;

static String queryArg(httpd_req_t* req, const char* key) {
  size_t len = httpd_req_get_url_query_len(req);
  if (!len) return String();
  std::unique_ptr<char[]> q(new char[len + 1]);
  if (httpd_req_get_url_query_str(req, q.get(), len + 1) != ESP_OK) return String();
  char val[128];
  if (httpd_query_key_value(q.get(), key, val, sizeof(val)) != ESP_OK) return String();
  return String(val);
}

static bool hasQueryArg(httpd_req_t* req, const char* key) {
  size_t len = httpd_req_get_url_query_len(req);
  if (!len) return false;
  std::unique_ptr<char[]> q(new char[len + 1]);
  if (httpd_req_get_url_query_str(req, q.get(), len + 1) != ESP_OK) return false;
  char val[8];
  esp_err_t e = httpd_query_key_value(q.get(), key, val, sizeof(val));
  return e == ESP_OK || e == ESP_ERR_HTTPD_RESULT_TRUNC;
}

static bool readBody(httpd_req_t* req, String& out, size_t maxLen = 8192) {
  size_t len = req->content_len;
  if (len > maxLen) return false;
  std::unique_ptr<char[]> buf(new char[len + 1]);
  size_t got = 0;
  int timeouts = 0;
  while (got < len) {
    int r = httpd_req_recv(req, buf.get() + got, len - got);
    if (r == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts <= MAX_RECV_TIMEOUTS) continue;
    if (r <= 0) return false;
    timeouts = 0;
    got += r;
  }
  buf[len] = 0;
  out = buf.get();
  return true;
}

static esp_err_t sendJson(httpd_req_t* req, JsonDocument& doc, const char* status = "200 OK") {
  String out;
  serializeJson(doc, out);
  httpd_resp_set_status(req, status);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, out.c_str(), out.length());
}

static esp_err_t sendErr(httpd_req_t* req, const char* status, const char* msg) {
  JsonDocument d; d["error"] = msg;
  return sendJson(req, d, status);
}

// Copy a request header; false if absent or longer than the buffer.
static bool header(httpd_req_t* req, const char* name, char* out, size_t outLen) {
  size_t len = httpd_req_get_hdr_value_len(req, name);
  if (len == 0 || len >= outLen) return false;
  return httpd_req_get_hdr_value_str(req, name, out, outLen) == ESP_OK;
}

// CSRF guard. Browsers send Origin on every cross-origin POST/DELETE (forms included),
// and would attach a cached Basic-auth login, so another web page could otherwise drive
// the API. Reject state-changing requests whose Origin is not this host. Tools such as
// curl send no Origin and are allowed (they still need the password).
static bool sameOrigin(httpd_req_t* req) {
  if (req->method == HTTP_GET) return true;
  if (httpd_req_get_hdr_value_len(req, "Origin") == 0) return true;
  char origin[160], host[128];
  if (!header(req, "Origin", origin, sizeof(origin)) || !header(req, "Host", host, sizeof(host))) return false;
  const char* o = origin;
  if (!strncasecmp(o, "https://", 8)) o += 8;
  else if (!strncasecmp(o, "http://", 7)) o += 7;
  else return false;  // includes "null"
  return strcasecmp(o, host) == 0;
}

// HTTP Basic auth as user "admin" when a web password is set.
static bool auth(httpd_req_t* req) {
  if (!sameOrigin(req)) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "Cross-origin request refused", HTTPD_RESP_USE_STRLEN);
    return false;
  }
  String pass;
  { AppGuard g; pass = settings.webPass; }
  if (pass.isEmpty()) return true;
  char hdr[160];
  if (httpd_req_get_hdr_value_str(req, "Authorization", hdr, sizeof(hdr)) == ESP_OK && strncmp(hdr, "Basic ", 6) == 0) {
    unsigned char dec[128]; size_t n = 0;
    if (mbedtls_base64_decode(dec, sizeof(dec) - 1, &n, (const unsigned char*)hdr + 6, strlen(hdr + 6)) == 0) {
      dec[n] = 0;
      String expect = "admin:" + pass;
      if (expect == (const char*)dec) return true;
    }
  }
  httpd_resp_set_status(req, "401 Unauthorized");
  httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Temperature Guard\"");
  httpd_resp_send(req, "Login required", HTTPD_RESP_USE_STRLEN);
  return false;
}

#define REQUIRE_AUTH(req) do { if (!auth(req)) return ESP_OK; } while (0)

// ---------- handlers ----------

static esp_err_t hRoot(httpd_req_t* req) {
  REQUIRE_AUTH(req);
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  // Refuse to be framed, so another site can't trick clicks on Boost etc. (clickjacking).
  httpd_resp_set_hdr(req, "X-Frame-Options", "DENY");
  httpd_resp_set_hdr(req, "Content-Security-Policy", "frame-ancestors 'none'");
  return httpd_resp_send(req, WEB_UI, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t hStatus(httpd_req_t* req) {
  REQUIRE_AUTH(req);
  JsonDocument d;
  {
    AppGuard g;
    time_t now = time(nullptr);
    if (!isnan(state.temp)) d["temp"] = roundf(state.temp * 10) / 10; else d["temp"] = nullptr;
    if (!isnan(state.hum)) d["hum"] = roundf(state.hum * 10) / 10; else d["hum"] = nullptr;
    d["sensorAgeS"] = state.lastGoodReadMs ? (int32_t)((millis() - state.lastGoodReadMs) / 1000) : -1;
    d["sensorErrors"] = state.sensorErrors;
    d["timeValid"] = state.timeValid;
    d["now"] = (uint32_t)now;
    if (state.timeValid) {
      struct tm tm; localtime_r(&now, &tm);
      char b[32]; strftime(b, sizeof(b), "%a %d %b %Y %H:%M:%S", &tm);
      d["localTime"] = b;
    }
    d["window"] = state.window;
    d["enabled"] = settings.enabled;
    d["heaterOn"] = state.heaterOn;
    d["heaterKnown"] = state.heaterKnown;
    d["heaterChangedAt"] = (uint32_t)state.heaterChangedAt;
    d["heaterMode"] = settings.heaterMode;
    d["reason"] = state.reason;
    d["wantOn"] = state.wantOn;
    d["lastCmd"] = state.lastCmdResult;
    d["lastCmdTime"] = (uint32_t)state.lastCmdTime;
    d["boostLeftS"] = state.boostUntilMs ? (int32_t)(state.boostUntilMs - millis()) / 1000 : 0;
    d["testHoldS"] = state.testHoldUntilMs ? max<int32_t>(0, (int32_t)(state.testHoldUntilMs - millis()) / 1000) : 0;
    d["onTemp"] = settings.onTemp;
    d["offTemp"] = settings.offTemp;
    d["nightStart"] = settings.nightStart;
    d["nightEnd"] = settings.nightEnd;
    d["weekendAllDay"] = settings.weekendAllDay;
    d["retentionDays"] = settings.retentionDays;
    d["logIntervalS"] = settings.logIntervalS;
    d["hostname"] = settings.hostname;
    d["title"] = settings.title;
    d["webPassSet"] = !settings.webPass.isEmpty();
    d["sd"] = datalog::sdReady();
    d["sdTotalMB"] = (uint32_t)(datalog::totalBytes() / 1048576ULL);
    d["sdUsedMB"] = (uint32_t)(datalog::usedBytes() / 1048576ULL);
    d["rssi"] = WiFi.RSSI();
    d["ip"] = WiFi.localIP().toString();
    d["uptimeS"] = millis() / 1000;
    d["heap"] = ESP.getFreeHeap();
    d["fw"] = FW_VERSION;
    JsonArray ev = d["events"].to<JsonArray>();
    for (size_t i = datalog::eventCount(); i-- > 0;) ev.add(datalog::eventAt(i));
  }
  return sendJson(req, d);
}

static esp_err_t hHistory(httpd_req_t* req) {
  REQUIRE_AUTH(req);
  // Copy under the lock, then send without it, so a slow client can't stall the control loop.
  std::vector<datalog::Sample> copy;
  {
    AppGuard g;
    copy.reserve(datalog::ringCount());
    for (size_t i = 0; i < datalog::ringCount(); i++) copy.push_back(datalog::ringAt(i));
  }
  httpd_resp_set_type(req, "text/csv");
  String chunk;
  chunk.reserve(2048);
  for (const auto& s : copy) {
    char line[64];
    snprintf(line, sizeof(line), "%lu,%.1f,%.1f,%u,%u\n", (unsigned long)s.t, s.temp10 / 10.0, s.hum10 / 10.0, s.heater, s.window);
    chunk += line;
    if (chunk.length() > 1800) { httpd_resp_send_chunk(req, chunk.c_str(), chunk.length()); chunk = ""; }
  }
  if (chunk.length()) httpd_resp_send_chunk(req, chunk.c_str(), chunk.length());
  return httpd_resp_send_chunk(req, nullptr, 0);
}

static void settingsToJson(JsonDocument& d) {
  d["enabled"] = settings.enabled;
  d["onTemp"] = settings.onTemp;
  d["offTemp"] = settings.offTemp;
  d["tempOffset"] = settings.tempOffset;
  d["nightStart"] = settings.nightStart;
  d["nightEnd"] = settings.nightEnd;
  d["weekendAllDay"] = settings.weekendAllDay;
  d["logIntervalS"] = settings.logIntervalS;
  d["retentionDays"] = settings.retentionDays;
  d["heaterMode"] = settings.heaterMode;
  d["tuyaId"] = settings.tuyaId;
  d["tuyaKey"] = settings.tuyaKey.isEmpty() ? "" : MASK;
  d["tuyaIp"] = settings.tuyaIp;
  d["tuyaVer"] = settings.tuyaVer;
  d["tuyaDp"] = settings.tuyaDp;
  d["hookOn"] = settings.hookOn.isEmpty() ? "" : MASK;   // URLs carry the service token
  d["hookOff"] = settings.hookOff.isEmpty() ? "" : MASK;
  d["ledLevel"] = settings.ledLevel;
  d["title"] = settings.title;
  d["wifiSsid"] = settings.wifiSsid;
  d["wifiPass"] = settings.wifiPass.isEmpty() ? "" : MASK;
  d["hostname"] = settings.hostname;
  d["webPass"] = settings.webPass.isEmpty() ? "" : MASK;
}

static esp_err_t hGetSettings(httpd_req_t* req) {
  REQUIRE_AUTH(req);
  JsonDocument d;
  { AppGuard g; settingsToJson(d); }
  return sendJson(req, d);
}

static void setSecret(String& dst, JsonVariant v) {
  if (v.isNull()) return;
  String s = v.as<String>();
  if (s != MASK) dst = s;
}

// Integer field: absent keeps the current value; present must be a whole number in [lo, hi]
// (ArduinoJson would otherwise turn out-of-range or fractional values into 0).
template <typename T>
static bool intField(JsonVariantConst v, T& dst, long lo, long hi) {
  if (v.isNull()) return true;
  if (!v.is<long>()) return false;
  long x = v.as<long>();
  if (x < lo || x > hi) return false;
  dst = (T)x;
  return true;
}

static bool numField(JsonVariantConst v, float& dst, float lo, float hi) {
  if (v.isNull()) return true;
  if (!v.is<float>()) return false;
  float x = v.as<float>();
  if (!(x >= lo && x <= hi)) return false;
  dst = x;
  return true;
}

static bool boolField(JsonVariantConst v, bool& dst) {
  if (v.isNull()) return true;
  if (!v.is<bool>()) return false;
  dst = v.as<bool>();
  return true;
}

static esp_err_t hPostSettings(httpd_req_t* req) {
  REQUIRE_AUTH(req);
  String body;
  if (!readBody(req, body)) return sendErr(req, "400 Bad Request", "body too large or unreadable");
  JsonDocument in;
  if (deserializeJson(in, body)) return sendErr(req, "400 Bad Request", "invalid JSON");

  AppGuard g;
  Settings n = settings;
  const char* bad = nullptr;
  if (!boolField(in["enabled"], n.enabled) || !boolField(in["weekendAllDay"], n.weekendAllDay)) bad = "Bad on/off value";
  else if (!numField(in["onTemp"], n.onTemp, -10, 40) || !numField(in["offTemp"], n.offTemp, -10, 40)) bad = "Temperatures must be between -10 and 40 °C";
  else if (!numField(in["tempOffset"], n.tempOffset, -10, 10)) bad = "Sensor correction must be between -10 and 10 °C";
  else if (!intField(in["nightStart"], n.nightStart, 0, 1439) || !intField(in["nightEnd"], n.nightEnd, 0, 1439)) bad = "Bad schedule time";
  else if (!intField(in["logIntervalS"], n.logIntervalS, 10, 3600)) bad = "Log interval must be 10-3600 s";
  else if (!intField(in["retentionDays"], n.retentionDays, 1, 3650)) bad = "Retention must be 1-3650 days";
  else if (!intField(in["heaterMode"], n.heaterMode, HEATER_NONE, HEATER_WEBHOOK)) bad = "Bad heater mode";
  else if (!intField(in["tuyaDp"], n.tuyaDp, 1, 255)) bad = "Switch DP must be 1-255";
  else if (!intField(in["ledLevel"], n.ledLevel, 0, 3)) bad = "Bad status light level";
  if (bad) return sendErr(req, "400 Bad Request", bad);
  if (!in["tuyaId"].isNull()) n.tuyaId = in["tuyaId"].as<String>();
  setSecret(n.tuyaKey, in["tuyaKey"]);
  if (!in["tuyaIp"].isNull()) n.tuyaIp = in["tuyaIp"].as<String>();
  if (!in["tuyaVer"].isNull()) n.tuyaVer = in["tuyaVer"].as<String>();
  setSecret(n.hookOn, in["hookOn"]);
  setSecret(n.hookOff, in["hookOff"]);
  if (!in["title"].isNull()) n.title = in["title"].as<String>();
  if (!in["wifiSsid"].isNull()) n.wifiSsid = in["wifiSsid"].as<String>();
  setSecret(n.wifiPass, in["wifiPass"]);
  if (!in["hostname"].isNull()) n.hostname = in["hostname"].as<String>();
  setSecret(n.webPass, in["webPass"]);

  n.tuyaId.trim(); n.tuyaKey.trim(); n.tuyaIp.trim(); n.hookOn.trim(); n.hookOff.trim(); n.hostname.trim(); n.title.trim();

  if (n.offTemp <= n.onTemp) return sendErr(req, "400 Bad Request", "Off temperature must be higher than on temperature");
  if (n.heaterMode == HEATER_TUYA && !n.tuyaKey.isEmpty() && n.tuyaKey.length() != 16) return sendErr(req, "400 Bad Request", "Tuya local key must be 16 characters");
  if (n.tuyaVer != "3.3" && n.tuyaVer != "3.4") return sendErr(req, "400 Bad Request", "Tuya version must be 3.3 or 3.4");
  if (n.wifiSsid.isEmpty()) return sendErr(req, "400 Bad Request", "WiFi SSID required");
  if (!n.wifiPass.isEmpty() && (n.wifiPass.length() < 8 || n.wifiPass.length() > 63))
    return sendErr(req, "400 Bad Request", "WiFi password must be 8-63 characters (empty for an open network)");
  if (n.title.isEmpty() || n.title.length() > 40) return sendErr(req, "400 Bad Request", "Name must be 1-40 characters");
  n.hostname.toLowerCase();
  bool hostOk = n.hostname.length() >= 1 && n.hostname.length() <= 32 && n.hostname[0] != '-' && !n.hostname.endsWith("-");
  for (char c : n.hostname) if (!isalnum((unsigned char)c) && c != '-') hostOk = false;
  if (!hostOk) return sendErr(req, "400 Bad Request", "Web address must be 1-32 letters, digits or hyphens");
  if (n.webPass.length() > 63) return sendErr(req, "400 Bad Request", "Web password must be at most 63 characters");

  bool hostChanged = n.hostname != settings.hostname;  // certificate names the host: restart to regenerate
  bool titleChanged = n.title != settings.title;
  bool netChanged = hostChanged || n.wifiSsid != settings.wifiSsid || n.wifiPass != settings.wifiPass;
  settings = n;
  settingsSave();
  controllerKick();  // retry the heater straight away with the new settings
  if (titleChanged && !hostChanged) MDNS.setInstanceName(settings.title.c_str());
  datalog::event(time(nullptr), "Settings saved");

  JsonDocument d; settingsToJson(d);
  d["netChanged"] = netChanged;
  if (hostChanged) rebootAtMs = millis() + 1500;
  else if (netChanged) wantNetApply = true;  // applied from loop() after this response is sent
  return sendJson(req, d);
}

static esp_err_t hHeater(httpd_req_t* req) {
  REQUIRE_AUTH(req);
  String cmd = queryArg(req, "cmd");
  if (cmd != "on" && cmd != "off") return sendErr(req, "400 Bad Request", "cmd must be on|off");
  JsonDocument d;
  { AppGuard g; d["ok"] = heaterTest(cmd == "on"); d["result"] = state.lastCmdResult; }
  return sendJson(req, d);
}

static esp_err_t hBoost(httpd_req_t* req) {
  REQUIRE_AUTH(req);
  int m = queryArg(req, "minutes").toInt();
  if (m < 0 || m > 720) return sendErr(req, "400 Bad Request", "minutes 0-720");
  { AppGuard g; boost(m); }
  JsonDocument d; d["ok"] = true;
  return sendJson(req, d);
}

static bool validLogName(const String& f) {
  if (f.length() < 5 || f.length() > 40 || f.indexOf('/') >= 0 || f.indexOf('\\') >= 0 || f.indexOf("..") >= 0) return false;
  return f.endsWith(".csv");
}

static esp_err_t hLogs(httpd_req_t* req) {
  REQUIRE_AUTH(req);
  JsonDocument d;
  JsonArray arr = d["files"].to<JsonArray>();
  {
    AppGuard g;
    if (datalog::sdReady()) {
      File dir = SD.open("/logs");
      if (dir) {
        for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
          if (!e.isDirectory()) { JsonObject o = arr.add<JsonObject>(); o["name"] = String(e.name()); o["size"] = e.size(); }
          e.close();
        }
        dir.close();
      }
    }
    d["sd"] = datalog::sdReady();
  }
  return sendJson(req, d);
}

static esp_err_t hLogFile(httpd_req_t* req) {
  REQUIRE_AUTH(req);
  String f = queryArg(req, "f");
  if (!validLogName(f)) return sendErr(req, "400 Bad Request", "bad file name");
  String path = "/logs/" + f;
  {
    AppGuard g;
    if (!datalog::sdReady() || !SD.exists(path)) return sendErr(req, "404 Not Found", "not found");
    if (req->method == HTTP_DELETE) {
      JsonDocument d; d["ok"] = SD.remove(path);
      return sendJson(req, d);
    }
  }
  // Read a chunk under the lock (open, seek, read, close), then send it without the lock,
  // so a slow client can't stall the control loop and no file handle stays open while
  // the logger appends to the same file.
  const size_t CHUNK = 4096;
  std::unique_ptr<char[]> buf(new char[CHUNK]);
  String disp;
  size_t pos = 0;
  for (;;) {
    size_t n = 0;
    bool opened = false;
    {
      AppGuard g;
      File file = datalog::sdReady() ? SD.open(path, FILE_READ) : File();
      if (file) {
        opened = true;
        if (pos == 0 || file.seek(pos)) n = file.read((uint8_t*)buf.get(), CHUNK);
        file.close();
      }
    }
    if (pos == 0) {
      if (!opened) return sendErr(req, "500 Internal Server Error", "open failed");
      httpd_resp_set_type(req, "text/csv");
      if (hasQueryArg(req, "download")) {
        disp = "attachment; filename=\"" + f + "\"";
        httpd_resp_set_hdr(req, "Content-Disposition", disp.c_str());
      }
    }
    if (n == 0) break;  // end of file (or the file vanished mid-download)
    if (httpd_resp_send_chunk(req, buf.get(), n) != ESP_OK) return ESP_FAIL;
    pos += n;
  }
  return httpd_resp_send_chunk(req, nullptr, 0);
}

static esp_err_t hReboot(httpd_req_t* req) {
  REQUIRE_AUTH(req);
  rebootAtMs = millis() + 800;
  JsonDocument d; d["ok"] = true;
  return sendJson(req, d);
}

// Raw firmware image in the request body:
//   curl -k --data-binary @firmware.bin https://temp-guard.local/api/update
static esp_err_t hUpdate(httpd_req_t* req) {
  REQUIRE_AUTH(req);
  size_t total = req->content_len;
  if (total < 100000) return sendErr(req, "400 Bad Request", "send the firmware .bin as the request body");
  if (!Update.begin(total)) return sendErr(req, "500 Internal Server Error", Update.errorString());
  { AppGuard g; datalog::event(time(nullptr), "Firmware update started (" + String(total) + " bytes)"); }
  std::unique_ptr<uint8_t[]> buf(new uint8_t[4096]);
  size_t got = 0;
  int timeouts = 0;
  while (got < total) {
    int r = httpd_req_recv(req, (char*)buf.get(), min((size_t)4096, total - got));
    if (r == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts <= MAX_RECV_TIMEOUTS) continue;
    if (r <= 0) { Update.abort(); return sendErr(req, "400 Bad Request", "upload interrupted"); }
    timeouts = 0;
    if (Update.write(buf.get(), r) != (size_t)r) { Update.abort(); return sendErr(req, "500 Internal Server Error", Update.errorString()); }
    got += r;
  }
  if (!Update.end(true)) return sendErr(req, "500 Internal Server Error", Update.errorString());
  Serial.printf("[OTA] OK, %u bytes\n", (unsigned)total);
  rebootAtMs = millis() + 800;
  JsonDocument d; d["ok"] = true; d["bytes"] = (uint32_t)total;
  return sendJson(req, d);
}

static esp_err_t hCert(httpd_req_t* req) {
  // Public: lets you compare the fingerprint with the one the browser shows.
  JsonDocument d;
  d["sha256"] = tlscert::fingerprintSha256();
  d["pem"] = tlscert::certPem();
  return sendJson(req, d);
}

static esp_err_t hNotFound(httpd_req_t* req, httpd_err_code_t) {
  httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
  return ESP_FAIL;
}

// Port 80: redirect everything to HTTPS on the same host.
static esp_err_t hRedirect(httpd_req_t* req) {
  Serial.printf("[WEB] redirect %s\n", req->uri);
  char host[64] = "";
  if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK || !host[0]) {
    strlcpy(host, WiFi.localIP().toString().c_str(), sizeof(host));
  }
  char* colon = strchr(host, ':');
  if (colon) *colon = 0;
  String loc = String("https://") + host + req->uri;
  httpd_resp_set_status(req, "301 Moved Permanently");
  httpd_resp_set_hdr(req, "Location", loc.c_str());
  return httpd_resp_send(req, nullptr, 0);
}

// ---------- startup ----------

static void reg(httpd_handle_t h, const char* uri, httpd_method_t m, esp_err_t (*fn)(httpd_req_t*)) {
  httpd_uri_t u = {};
  u.uri = uri; u.method = m; u.handler = fn;
  httpd_register_uri_handler(h, &u);
}

// staIp empty = setup access point only.
static bool startServers(const String& host, const String& staIp) {
  if (!tlscert::ensure(host, staIp)) return false;
  String ip = staIp.length() ? staIp : WiFi.softAPIP().toString();

  httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
  conf.httpd.max_uri_handlers = 16;
  conf.httpd.stack_size = 12288;
  conf.httpd.max_open_sockets = 3;
  conf.httpd.recv_wait_timeout = 10;
  conf.httpd.send_wait_timeout = 10;
  conf.transport_mode = HTTPD_SSL_TRANSPORT_SECURE;
  conf.cacert_pem = (const uint8_t*)tlscert::certPem().c_str();
  conf.cacert_len = tlscert::certPem().length() + 1;
  conf.prvtkey_pem = (const uint8_t*)tlscert::keyPem().c_str();
  conf.prvtkey_len = tlscert::keyPem().length() + 1;
  esp_err_t e = httpd_ssl_start(&https, &conf);
  if (e != ESP_OK) { Serial.printf("[WEB] HTTPS start failed: %s\n", esp_err_to_name(e)); https = nullptr; return false; }

  reg(https, "/", HTTP_GET, hRoot);
  reg(https, "/api/status", HTTP_GET, hStatus);
  reg(https, "/api/history", HTTP_GET, hHistory);
  reg(https, "/api/settings", HTTP_GET, hGetSettings);
  reg(https, "/api/settings", HTTP_POST, hPostSettings);
  reg(https, "/api/heater", HTTP_POST, hHeater);
  reg(https, "/api/boost", HTTP_POST, hBoost);
  reg(https, "/api/logs", HTTP_GET, hLogs);
  reg(https, "/api/log", HTTP_GET, hLogFile);
  reg(https, "/api/log", HTTP_DELETE, hLogFile);
  reg(https, "/api/reboot", HTTP_POST, hReboot);
  reg(https, "/api/update", HTTP_POST, hUpdate);
  reg(https, "/api/cert", HTTP_GET, hCert);
  httpd_register_err_handler(https, HTTPD_404_NOT_FOUND, hNotFound);

  httpd_config_t pc = HTTPD_DEFAULT_CONFIG();
  pc.server_port = 80;
  pc.ctrl_port = 32769;
  pc.max_open_sockets = 2;
  pc.uri_match_fn = httpd_uri_match_wildcard;
  esp_err_t pe = httpd_start(&http, &pc);
  Serial.printf("[WEB] HTTP redirect server on :80: %s\n", esp_err_to_name(pe));
  if (pe == ESP_OK) {
    reg(http, "/*", HTTP_GET, hRedirect);
    reg(http, "/*", HTTP_POST, hRedirect);
  } else {
    http = nullptr;
  }
  Serial.printf("[WEB] HTTPS up: https://%s.local  https://%s\n", host.c_str(), ip.c_str());
  Serial.printf("[WEB] Cert SHA-256 %s\n", tlscert::fingerprintSha256().c_str());
  return true;
}

void webBegin() {}

static void stopServers() {
  if (https) { httpd_ssl_stop(https); https = nullptr; }
  if (http) { httpd_stop(http); http = nullptr; }
}

void webService(bool staUp, bool apUp, const String& staIp) {
  static uint32_t lastTry = 0, lastCheck = 0, staSince = 0;
  uint32_t ms = millis();
  if (!staUp) staSince = 0;
  else if (!staSince) staSince = ms ? ms : 1;
  // After joining WiFi, give SNTP up to 30 s so a new certificate gets real dates
  // (otherwise it is dated from the firmware build).
  bool settling = staUp && time(nullptr) < 1700000000 && ms - staSince < 30000;

  if (https) {
    // In setup-AP-only mode keep whatever certificate we have (it also names 192.168.4.1).
    if (!staUp || settling || ms - lastCheck < 10000) return;
    lastCheck = ms;
    String host;
    { AppGuard g; host = settings.hostname; }
    if (!tlscert::stale(host, staIp)) return;
    Serial.println("[WEB] Certificate out of date (new IP or near expiry): restarting server");
    stopServers();
    lastTry = 0;
  }
  if ((!staUp && !apUp) || settling) return;
  if (lastTry && ms - lastTry < 10000) return;
  lastTry = ms ? ms : 1;
  String host;
  { AppGuard g; host = settings.hostname; }
  if (!startServers(host, staUp ? staIp : String())) stopServers();
}

void webTick() {
  if (wantNetApply) { wantNetApply = false; delay(300); applyNetworkSettings(); }
  if (rebootAtMs && (int32_t)(millis() - rebootAtMs) > 0) ESP.restart();
}
