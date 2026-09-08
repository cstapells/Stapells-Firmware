#include "stapells/ConfigStore.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "stapells/SiteDefaults.h"

namespace stapells {

bool ConfigStore::begin() {
  applySiteDefaults(config_);
  mounted_ = LittleFS.begin();
  if (!mounted_) {
    Serial.println(F("[core] LittleFS mount failed"));
    return false;
  }
  return load();
}

bool ConfigStore::load() {
  if (!mounted_ || !LittleFS.exists(kPath)) return true;

  File file = LittleFS.open(kPath, "r");
  if (!file) return false;

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.printf("[core] Config parse failed: %s\n", error.c_str());
    return false;
  }

  config_.wifiSsid = doc["wifi"]["ssid"] | "";
  config_.wifiPassword = doc["wifi"]["password"] | "";
  config_.mqttHost = doc["mqtt"]["host"] | "";
  config_.mqttPort = doc["mqtt"]["port"] | 1883;
  config_.mqttUsername = doc["mqtt"]["username"] | "";
  config_.mqttPassword = doc["mqtt"]["password"] | "";
  config_.otaBaseUrl = doc["ota"]["baseUrl"] | config_.otaBaseUrl;
  config_.topicRoot = doc["mqtt"]["topicRoot"] | "Control";
  config_.nodeName = doc["nodeName"] | "";
  config_.healthLedPin = doc["health"]["gpio"] | -1;
  config_.healthBrightness = doc["health"]["brightness"] | 24;
  return true;
}

bool ConfigStore::save() {
  if (!mounted_) return false;

  JsonDocument doc;
  doc["schema"] = 1;
  doc["wifi"]["ssid"] = config_.wifiSsid;
  doc["wifi"]["password"] = config_.wifiPassword;
  doc["mqtt"]["host"] = config_.mqttHost;
  doc["mqtt"]["port"] = config_.mqttPort;
  doc["mqtt"]["username"] = config_.mqttUsername;
  doc["mqtt"]["password"] = config_.mqttPassword;
  doc["ota"]["baseUrl"] = config_.otaBaseUrl;
  doc["mqtt"]["topicRoot"] = config_.topicRoot;
  doc["nodeName"] = config_.nodeName;
  doc["health"]["gpio"] = config_.healthLedPin;
  doc["health"]["brightness"] = config_.healthBrightness;

  File file = LittleFS.open(kPath, "w");
  if (!file) return false;
  const bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

bool ConfigStore::updateFromJson(const String& body, String& error) {
  JsonDocument doc;
  const DeserializationError jsonError = deserializeJson(doc, body);
  if (jsonError) {
    error = String("Invalid JSON: ") + jsonError.c_str();
    return false;
  }

  CoreConfig candidate = config_;
  if (doc["wifiSsid"].is<const char*>()) candidate.wifiSsid = doc["wifiSsid"].as<String>();
  if (doc["wifiPassword"].is<const char*>()) candidate.wifiPassword = doc["wifiPassword"].as<String>();
  if (doc["mqttHost"].is<const char*>()) candidate.mqttHost = doc["mqttHost"].as<String>();
  if (doc["mqttPort"].is<uint16_t>()) candidate.mqttPort = doc["mqttPort"].as<uint16_t>();
  if (doc["mqttUsername"].is<const char*>()) candidate.mqttUsername = doc["mqttUsername"].as<String>();
  if (doc["mqttPassword"].is<const char*>()) candidate.mqttPassword = doc["mqttPassword"].as<String>();
  if (doc["topicRoot"].is<const char*>()) candidate.topicRoot = doc["topicRoot"].as<String>();
  if (doc["otaBaseUrl"].is<const char*>()) candidate.otaBaseUrl = doc["otaBaseUrl"].as<String>();
  if (doc["nodeName"].is<const char*>()) candidate.nodeName = doc["nodeName"].as<String>();
  if (doc["healthLedPin"].is<int>()) candidate.healthLedPin = doc["healthLedPin"].as<int>();
  if (doc["healthBrightness"].is<uint8_t>()) candidate.healthBrightness = doc["healthBrightness"].as<uint8_t>();

  candidate.wifiSsid.trim();
  candidate.mqttHost.trim();
  candidate.otaBaseUrl.trim();
  candidate.topicRoot.trim();
  if (candidate.wifiSsid.isEmpty()) {
    error = "WIFI_NAME_REQUIRED";
    return false;
  }
  if (candidate.wifiSsid.length() > 32 || candidate.wifiPassword.length() > 63) {
    error = "WIFI_SETTINGS_TOO_LONG";
    return false;
  }
  if (candidate.mqttHost.isEmpty()) {
    error = "MQTT_SERVER_REQUIRED";
    return false;
  }
  if (candidate.mqttHost.length() > 128 || candidate.mqttUsername.length() > 64 ||
      candidate.mqttPassword.length() > 128) {
    error = "MQTT_SETTINGS_TOO_LONG";
    return false;
  }
  if (candidate.mqttPort == 0) {
    error = "mqttPort must be between 1 and 65535";
    return false;
  }
  if (candidate.topicRoot.isEmpty()) candidate.topicRoot = "Control";
  if (candidate.topicRoot.length() > 64 || candidate.otaBaseUrl.length() > 160) {
    error = "SERVER_SETTINGS_TOO_LONG";
    return false;
  }
  if (!candidate.otaBaseUrl.startsWith("http://") ||
      candidate.otaBaseUrl.indexOf(' ') >= 0) {
    error = "OTA_SERVER_INVALID";
    return false;
  }
  config_ = candidate;
  return save();
}

bool ConfigStore::factoryReset() {
  config_ = CoreConfig{};
  return !mounted_ || !LittleFS.exists(kPath) || LittleFS.remove(kPath);
}

}  // namespace stapells

