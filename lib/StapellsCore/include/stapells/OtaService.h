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

 private:
  void publish(const String& state, const String& detail = "");

  const CoreConfig* config_{nullptr};
  const PlatformInfo* platform_{nullptr};
  MqttService* mqtt_{nullptr};
  String requestedVersion_{};
  String requestId_{};
  bool pending_{false};
};

}  // namespace stapells
