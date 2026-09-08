#include "stapells/OtaService.h"

#include <ArduinoJson.h>

#if defined(ESP8266)
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>
#else
#include <HTTPUpdate.h>
#include <WiFi.h>
#endif

#ifndef STAPELLS_BOARD_TARGET
#define STAPELLS_BOARD_TARGET "unknown"
#endif

namespace stapells {

void OtaService::begin(const CoreConfig& config, const PlatformInfo& platform,
                       MqttService& mqtt) {
  config_ = &config;
  platform_ = &platform;
  mqtt_ = &mqtt;
}

bool OtaService::accept(const String& payload) {
  if (!config_ || !platform_ || !mqtt_ || pending_) return false;
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    publish("REJECTED", "The update request was not valid JSON.");
    return false;
  }
  const String target = doc["target"] | "";
  const String confirmation = doc["confirm"] | "";
  const String version = doc["version"] | "";
  const String requestId = doc["requestId"] | "";
  if (target != STAPELLS_BOARD_TARGET || confirmation != "APPLY" ||
      version.isEmpty() || requestId.isEmpty()) {
    publish("REJECTED", "Board type or confirmation did not match.");
    return false;
  }
  requestedVersion_ = version;
  requestId_ = requestId;
  pending_ = true;
  publish("ACCEPTED", String("Preparing ") + requestedVersion_);
  return true;
}

void OtaService::publish(const String& state, const String& detail) {
  if (!mqtt_) return;
  JsonDocument doc;
  doc["state"] = state;
  doc["requestId"] = requestId_;
  doc["version"] = requestedVersion_;
  if (!detail.isEmpty()) doc["detail"] = detail;
  String payload;
  serializeJson(doc, payload);
  mqtt_->publish("ota/status", payload, true);
}

bool OtaService::run(bool networkReady) {
  if (!pending_ || !networkReady || !config_) return false;
  pending_ = false;
  String url = config_->otaBaseUrl;
  if (url.endsWith("/")) url.remove(url.length() - 1);
  url += "/api/firmware/download?kind=ota&target=";
  url += STAPELLS_BOARD_TARGET;
  url += "&release=";
  url += requestedVersion_;
  publish("DOWNLOADING", "Downloading the board-matched OTA image.");
  delay(75);

  WiFiClient client;
#if defined(ESP8266)
  ESPhttpUpdate.rebootOnUpdate(true);
  const t_httpUpdate_return result =
      ESPhttpUpdate.update(client, url, STAPELLS_FIRMWARE_VERSION);
  const String error = ESPhttpUpdate.getLastErrorString();
#else
  httpUpdate.rebootOnUpdate(true);
  const t_httpUpdate_return result =
      httpUpdate.update(client, url, STAPELLS_FIRMWARE_VERSION);
  const String error = httpUpdate.getLastErrorString();
#endif

  if (result == HTTP_UPDATE_NO_UPDATES) publish("CURRENT", "This firmware is already current.");
  else if (result == HTTP_UPDATE_FAILED) publish("FAILED", error);
  return result == HTTP_UPDATE_OK;
}

}  // namespace stapells
