#pragma once

#include <Arduino.h>

#include "stapells/ConfigStore.h"
#include "stapells/MqttService.h"
#include "stapells/Platform.h"

namespace stapells {

class OtaService {
 public:
  void begin(const CoreConfig& config, const PlatformInfo& platform, MqttService& mqtt);
  bool accept(const String& payload);
  bool pending() const { return pending_; }
  bool run(bool networkReady);
  void loop();

 private:
  void publish(const String& state, const String& detail = "");
  bool saveAttempt(const String& state, const String& imageMd5 = "");

  const CoreConfig* config_{nullptr};
  const PlatformInfo* platform_{nullptr};
  MqttService* mqtt_{nullptr};
  String requestedVersion_{};
  String requestId_{};
  bool pending_{false};
  String state_{};
  String detail_{};
  String imageMd5_{};
  int progress_{-1};
  uint32_t reportedConnection_{0};
  uint32_t lastReportMs_{0};
};

}  // namespace stapells
