#pragma once
#include <Arduino.h>
#include <FS.h>

// Rolling CSV logger on the SPI SD card: one file per day in /logs/YYYY-MM-DD.csv,
// oldest files pruned by retention days and whenever the card gets nearly full.
// Also keeps a RAM ring buffer of recent samples for the web chart.
namespace datalog {

struct Sample { uint32_t t; int16_t temp10; int16_t hum10; uint8_t heater; uint8_t window; };

bool begin(uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t cs);
bool sdReady();
void addSample(time_t t, float temp, float hum, bool heater, bool window);
void event(time_t t, const String& msg);
void prune(uint16_t retentionDays);
// Refill the RAM ring with the last 24h from SD (call once time is valid).
void loadRecent(time_t now);
uint64_t totalBytes();
uint64_t usedBytes();

// RAM ring (most recent last)
size_t ringCount();
const Sample& ringAt(size_t i);

// Recent events (RAM, most recent last)
size_t eventCount();
const String& eventAt(size_t i);

fs::FS& fs();
}
