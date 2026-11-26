#pragma once
#include <Arduino.h>

// Self-signed ECDSA P-256 certificate for the HTTPS UI, generated on the device
// (the private key never leaves it) and kept in NVS. It names <host>.local, <host>,
// the WiFi IP and the setup access point's 192.168.4.1, and is regenerated when the
// hostname or WiFi IP no longer match, or when it is within 30 days of expiry.
namespace tlscert {
// Ensures a valid cert exists. ip = the WiFi (station) IP, or empty when only the setup
// access point is up: then any existing certificate for this hostname is kept.
bool ensure(const String& hostname, const String& ip);
// True if the loaded certificate should be replaced for this hostname/WiFi IP.
bool stale(const String& hostname, const String& ip);
const String& certPem();
const String& keyPem();
String fingerprintSha256();  // "AB:CD:..." for checking the browser warning
}
