#include "stapells/NetworkService.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

namespace stapells {
namespace {
constexpr uint32_t kConnectTimeoutMs = 15000;
constexpr uint32_t kRetryDelayMs = 10000;
}

void NetworkService::begin(const CoreConfig& config, const String& boardId) {
  config_ = &config;
  boardId_ = boardId;
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  if (config.hasWifi()) connectStation();
  else Serial.println(F("[wifi] No site Wi-Fi configured in SiteDefaults"));
}

void NetworkService::connectStation() {
  if (!config_ || !config_->hasWifi()) return;
  WiFi.mode(WIFI_STA);
#if defined(ESP8266)
  WiFi.hostname((String("stapells-") + boardId_).c_str());
#else
  WiFi.setHostname((String("stapells-") + boardId_).c_str());
#endif
  WiFi.begin(config_->wifiSsid.c_str(), config_->wifiPassword.c_str());
  attemptStartedMs_ = millis();
  nextAttemptMs_ = 0;
  Serial.printf("[wifi] Connecting to %s\n", config_->wifiSsid.c_str());
}

void NetworkService::loop() {
  if (stationConnected()) return;
  const uint32_t now = millis();

  if (!config_ || !config_->hasWifi()) {
    return;
  }

  if (attemptStartedMs_ != 0 && now - attemptStartedMs_ >= kConnectTimeoutMs) {
    attemptStartedMs_ = 0;
    nextAttemptMs_ = now + kRetryDelayMs;
    Serial.println(F("[wifi] Station timeout; retrying site Wi-Fi"));
  }

  if (attemptStartedMs_ == 0 && static_cast<int32_t>(now - nextAttemptMs_) >= 0) connectStation();
}

bool NetworkService::stationConnected() const { return WiFi.status() == WL_CONNECTED; }

String NetworkService::ipAddress() const {
  return stationConnected() ? WiFi.localIP().toString() : String();
}

int32_t NetworkService::rssi() const { return stationConnected() ? WiFi.RSSI() : 0; }

}  // namespace stapells

