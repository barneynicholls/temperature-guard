#include "datalog.h"
#include <SPI.h>
#include <SD.h>
#include <vector>
#include <algorithm>

namespace datalog {

static bool ready = false;
static const size_t RING_MAX = 1440;  // 24h at 1-minute interval
static Sample ring[RING_MAX];
static size_t ringHead = 0, ringLen = 0;

static const size_t EVT_MAX = 50;
static String evts[EVT_MAX];
static size_t evtHead = 0, evtLen = 0;

fs::FS& fs() { return SD; }
bool sdReady() { return ready; }

bool begin(uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t cs) {
  static bool spiInit = false;
  if (!spiInit) { SPI.begin(sck, miso, mosi, cs); spiInit = true; }
  SD.end();
  if (!SD.begin(cs, SPI, 4000000) || SD.cardType() == CARD_NONE) {
    Serial.println("[LOG] SD card not available");
    ready = false;
    return false;
  }
  if (!SD.exists("/logs")) SD.mkdir("/logs");
  Serial.printf("[LOG] SD ready, %llu MB\n", SD.cardSize() / (1024ULL * 1024ULL));
  ready = true;
  return true;
}

static String fmtTime(time_t t) {
  struct tm tm; localtime_r(&t, &tm);
  char b[24]; strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &tm);
  return b;
}

static String dayFile(time_t t) {
  struct tm tm; localtime_r(&t, &tm);
  char b[32]; strftime(b, sizeof(b), "/logs/%Y-%m-%d.csv", &tm);
  return b;
}

static void appendLine(time_t t, const String& line) {
  if (!ready) return;
  String path = dayFile(t);
  bool isNew = !SD.exists(path);
  File f = SD.open(path, FILE_APPEND);
  if (!f) {
    Serial.println("[LOG] open failed, will remount");
    ready = false;
    return;
  }
  if (isNew) f.println("time,temp_c,humidity_pct,heater,window,event");
  f.println(line);
  f.close();
}

static void pushRing(const Sample& s);

void addSample(time_t t, float temp, float hum, bool heater, bool window) {
  Sample s{(uint32_t)t, (int16_t)lroundf(temp * 10), (int16_t)lroundf(hum * 10), (uint8_t)heater, (uint8_t)window};
  pushRing(s);

  char line[96];
  snprintf(line, sizeof(line), "%s,%.1f,%.1f,%d,%d,", fmtTime(t).c_str(), temp, hum, heater ? 1 : 0, window ? 1 : 0);
  appendLine(t, line);
}

void event(time_t t, const String& msg) {
  String stamp = t > 1700000000 ? fmtTime(t) : String("uptime ") + String(millis() / 1000) + "s";
  Serial.printf("[EVT] %s %s\n", stamp.c_str(), msg.c_str());
  evts[evtHead] = stamp + "  " + msg;
  evtHead = (evtHead + 1) % EVT_MAX;
  if (evtLen < EVT_MAX) evtLen++;
  if (t > 1700000000) {
    String m = msg; m.replace(",", ";"); m.replace("\n", " ");
    appendLine(t, fmtTime(t) + ",,,,," + m);
  }
}

static std::vector<String> listLogs() {
  std::vector<String> names;
  File dir = SD.open("/logs");
  if (!dir) return names;
  for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
    if (!e.isDirectory()) names.push_back(String(e.name()));
    e.close();
  }
  dir.close();
  std::sort(names.begin(), names.end());  // YYYY-MM-DD sorts chronologically
  return names;
}

void prune(uint16_t retentionDays) {
  if (!ready) return;
  auto names = listLogs();
  time_t now = time(nullptr);
  if (now > 1700000000 && retentionDays > 0) {
    String cutoff = dayFile(now - (time_t)retentionDays * 86400).substring(6);  // "YYYY-MM-DD.csv"
    for (auto& n : names) {
      if (n < cutoff) { SD.remove("/logs/" + n); Serial.printf("[LOG] pruned old %s\n", n.c_str()); }
    }
    names = listLogs();
  }
  // Keep at least 10% of the card free; drop oldest days first (never today's file).
  size_t i = 0;
  while (SD.totalBytes() > 0 && SD.usedBytes() > SD.totalBytes() * 9 / 10 && i + 1 < names.size()) {
    SD.remove("/logs/" + names[i]);
    Serial.printf("[LOG] pruned for space %s\n", names[i].c_str());
    i++;
  }
}

static void pushRing(const Sample& s) {
  ring[ringHead] = s;
  ringHead = (ringHead + 1) % RING_MAX;
  if (ringLen < RING_MAX) ringLen++;
}

void loadRecent(time_t now) {
  if (!ready) return;
  ringHead = ringLen = 0;
  time_t since = now - 86400;
  for (time_t day : {since, now}) {
    File f = SD.open(dayFile(day), FILE_READ);
    if (!f) continue;
    while (f.available()) {
      String l = f.readStringUntil('\n');
      // "YYYY-MM-DD HH:MM:SS,temp,hum,heater,window,event"
      if (l.length() < 25 || l[19] != ',' || l[20] == ',') continue;
      struct tm tm = {};
      if (sscanf(l.c_str(), "%d-%d-%d %d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) continue;
      tm.tm_year -= 1900; tm.tm_mon -= 1; tm.tm_isdst = -1;
      time_t t = mktime(&tm);
      if (t < since || t > now) continue;
      float temp, hum; int h, w;
      if (sscanf(l.c_str() + 20, "%f,%f,%d,%d", &temp, &hum, &h, &w) != 4) continue;
      pushRing(Sample{(uint32_t)t, (int16_t)lroundf(temp * 10), (int16_t)lroundf(hum * 10), (uint8_t)h, (uint8_t)w});
    }
    f.close();
    if (dayFile(since) == dayFile(now)) break;
  }
  Serial.printf("[LOG] Restored %u samples from SD\n", (unsigned)ringLen);
}

uint64_t totalBytes() { return ready ? SD.totalBytes() : 0; }
uint64_t usedBytes() { return ready ? SD.usedBytes() : 0; }

size_t ringCount() { return ringLen; }
const Sample& ringAt(size_t i) { return ring[(ringHead + RING_MAX - ringLen + i) % RING_MAX]; }
size_t eventCount() { return evtLen; }
const String& eventAt(size_t i) { return evts[(evtHead + EVT_MAX - evtLen + i) % EVT_MAX]; }

}  // namespace datalog
