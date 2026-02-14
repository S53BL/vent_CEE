// example_wifi_config.h
// Kopirajte to datoteko v wifi_config.h in vnesite svoje prave podatke!

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <WiFi.h>

// Seznam WiFi omrežij (lahko jih imate več)
const char* ssidList[] = {
    "MojeOmrezje",
    "DrugoOmrezje",
    "TretjeOmrezje"
};

const char* passwordList[] = {
    "zamenjajte_me_123456",
    "zamenjajte_me_789012",
    "zamenjajte_me_567890"
};

const int numNetworks = 3;  // število omrežij v zgornjih seznamih

// Statična IP konfiguracija - PRIMER (spremenite po svoji mreži!)
IPAddress localIP(192, 168, 1, 100);     // primer lokalnega IP naslova
IPAddress gateway(192, 168, 1, 1);       // primer prehod (router)
IPAddress subnet(255, 255, 255, 0);      // primer maske podomrežja
IPAddress dns(8, 8, 8, 8);               // primer DNS (Google)

#endif