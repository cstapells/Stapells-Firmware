#include "stapells/OtaService.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#if defined(ESP8266)
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>
#else
#include <HTTPUpdate.h>
#include <Update.h>
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
  File file = LittleFS.open("/ota-attempt.json", "r");
  if (!file) return;
  JsonDocument doc;
  const auto error = deserializeJson(doc, file);
  file.close();
  if (error) {
    state_ = "UNCONFIRMED";
    detail_ = "The previous update record could not be read. Check the board before retrying.";
    return;
  }
  requestId_ = doc["requestId"] | "";
  requestedVersion_ = doc["version"] | "";
  state_ = doc["state"] | "UNCONFIRMED";
  imageMd5_ = doc["imageMd5"] | "";
  if (state_ == "REBOOTING" || state_ == "COMPLETE") {
    const bool matches = imageMd5_.length() == 32 && ESP.getSketchMD5().equalsIgnoreCase(imageMd5_);
    state_ = matches ? "COMPLETE" : "UNCONFIRMED";
    detail_ = matches ? "Restarted and verified the installed firmware image. Update complete."
                      : "Restarted, but the running image does not match the saved update record.";
  } else if (state_ == "INSTALLING") {
    state_ = "UNCONFIRMED";
    detail_ = "Restarted before the update recorded completion. The update may have been interrupted.";
  } else {
    detail_ = doc["detail"] | "Previous update did not install a new image.";
  }
}

bool OtaService::saveAttempt(const String& state, const String& imageMd5) {
  JsonDocument doc;
  doc["requestId"] = requestId_;
  doc["version"] = requestedVersion_;
  doc["state"] = state;
  doc["detail"] = detail_;
  doc["imageMd5"] = imageMd5;
  // Never format storage during OTA: it also contains private network settings.
  File file = LittleFS.open("/ota-attempt.tmp", "w");
  if (!file) return false;
  const bool ok = serializeJson(doc, file) == measureJson(doc);
  file.flush();
  file.close();
  return ok && LittleFS.rename("/ota-attempt.tmp", "/ota-attempt.json");
}

void OtaService::loop() {
  if (!mqtt_ || !mqtt_->connected() || state_.isEmpty()) return;
  if (reportedConnection_ != mqtt_->connectionCount() || millis() - lastReportMs_ >= 30000) {
    publish(state_, detail_);
  }
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
  progress_ = -1;
  if (!saveAttempt("INSTALLING")) {
    publish("FAILED", "Cannot save the update recovery record. No firmware download was started.");
    return false;
  }
  pending_ = true;
  publish("ACCEPTED", String("Preparing ") + requestedVersion_);
  return true;
}

void OtaService::publish(const String& state, const String& detail) {
  if (!mqtt_) return;
  state_ = state;
  detail_ = detail;
  JsonDocument doc;
  doc["state"] = state;
  doc["requestId"] = requestId_;
  doc["version"] = requestedVersion_;
  doc["runningVersion"] = STAPELLS_FIRMWARE_VERSION;
  doc["feedbackVersion"] = 2;
  if (progress_ >= 0) doc["progress"] = progress_;
  if (!detail.isEmpty()) doc["detail"] = detail;
  String payload;
  serializeJson(doc, payload);
  if (mqtt_->publish("ota/status", payload, true)) {
    reportedConnection_ = mqtt_->connectionCount();
    lastReportMs_ = millis();
  }
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
  auto progress = [this](int current, int total) {
    if (total <= 0) return;
    const int percent = static_cast<int>(100LL * current / total);
    if (percent >= progress_ + 5 || percent == 100) {
      progress_ = percent;
      publish("INSTALLING", "Receiving and writing firmware. Keep power connected; restart verification follows.");
    }
  };
#if defined(ESP8266)
  ESPhttpUpdate.closeConnectionsOnUpdate(false);
  ESPhttpUpdate.rebootOnUpdate(false);
  ESPhttpUpdate.onProgress(progress);
  const t_httpUpdate_return result =
      ESPhttpUpdate.update(client, url, STAPELLS_FIRMWARE_VERSION);
  const String error = ESPhttpUpdate.getLastErrorString();
#else
  httpUpdate.rebootOnUpdate(false);
  httpUpdate.onProgress(progress);
  const t_httpUpdate_return result =
      httpUpdate.update(client, url, STAPELLS_FIRMWARE_VERSION);
  const String error = httpUpdate.getLastErrorString();
#endif

  if (result == HTTP_UPDATE_OK) {
    imageMd5_ = Update.md5String();
    if (saveAttempt("REBOOTING", imageMd5_)) {
      publish("REBOOTING", "Firmware written. Restarting now; waiting to verify the running image.");
    } else {
      publish("UNCONFIRMED", "Firmware written, but the verification record could not be saved. Restarting; completion cannot be confirmed.");
    }
    delay(300);
    restartPlatform();
  } else {
    progress_ = -1;
    publish(result == HTTP_UPDATE_NO_UPDATES ? "CURRENT" : "FAILED",
            result == HTTP_UPDATE_NO_UPDATES ? "This firmware is already current." : error);
    saveAttempt(state_);
  }
  return result == HTTP_UPDATE_OK;
}

}  // namespace stapells
