#pragma once
#include <Arduino.h>

void webBegin();
// Runs deferred actions (WiFi re-apply, reboot) requested by web handlers.
// Call from loop() while holding the app lock.
void webTick();
// Starts the HTTPS server once the network is up, and restarts it with a new certificate
// when the IP changes or the certificate nears expiry. Call from loop() WITHOUT the app
// lock: stopping the server waits for running handlers, which may be waiting for the lock.
void webService(bool staUp, bool apUp, const String& staIp);

void applyNetworkSettings();  // main.cpp
