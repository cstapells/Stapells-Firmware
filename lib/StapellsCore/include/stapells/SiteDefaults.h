#pragma once

#include "stapells/ConfigStore.h"

// Development-only pass-through for site settings. These compile-time values
// deliberately sit behind one function so a controller-provided credential
// source can replace them without changing NetworkService or any function card.
#ifndef STAPELLS_WIFI_SSID
#define STAPELLS_WIFI_SSID ""
#endif

#ifndef STAPELLS_WIFI_PASSWORD
#define STAPELLS_WIFI_PASSWORD ""
#endif

#ifndef STAPELLS_MQTT_HOST
#define STAPELLS_MQTT_HOST "192.168.2.60"
#endif

#ifndef STAPELLS_OTA_BASE_URL
#define STAPELLS_OTA_BASE_URL "http://192.168.2.60:4173"
#endif

#ifndef STAPELLS_HEALTH_LED_PIN
#define STAPELLS_HEALTH_LED_PIN -1
#endif

namespace stapells {

inline void applySiteDefaults(CoreConfig& config) {
  config.wifiSsid = STAPELLS_WIFI_SSID;
  config.wifiPassword = STAPELLS_WIFI_PASSWORD;
  config.mqttHost = STAPELLS_MQTT_HOST;
  config.mqttPort = 1883;
  config.otaBaseUrl = STAPELLS_OTA_BASE_URL;
  config.topicRoot = "Control";
  config.healthLedPin = STAPELLS_HEALTH_LED_PIN;
}

}  // namespace stapells

