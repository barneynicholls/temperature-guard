#pragma once
#include <Arduino.h>

// Minimal Tuya local-LAN client (protocol 3.3 and 3.4) for switching one DP on a
// Smart Life / Tuya plug. Needs the device id, the 16-char local key and the IP.
namespace tuya {
// Returns true if the device acknowledged. `err` gets a short reason on failure.
bool setSwitch(const String& ip, const String& devId, const String& localKey,
               const String& version, uint8_t dp, bool on, String& err);
}
