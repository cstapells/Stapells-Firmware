#pragma once

#include <Arduino.h>

#include "stapells/ConfigStore.h"
#include "stapells/HealthStrip.h"
#include "stapells/MqttService.h"
#include "stapells/NetworkService.h"
#include "stapells/Platform.h"
#include "stapells/WebService.h"

namespace stapells {

enum class NodeState : uint8_t {
  Booting,
  ConnectingWifi,
  ConnectingMqtt,
  Unconfigured,
  Operational,
  Updating,
  Recovering,
  Fault
};

class Core {
 public:
  void begin();
  void loop();
  void activity();

  NodeState state() const { return state_; }
  const PlatformInfo& platform() const { return platform_; }
  const CoreConfig& config() const { return configStore_.config(); }

 private:
  static Core* instance_;
  static void onMqttMessage(const String& topic, const String& payload);
  static String statusJson();
  static bool saveConfigJson(const String& body, String& error);
  static void reboot();
  static void factoryReset();
  static void otaStart();
  static void otaEnd(bool success);

  void handleMqttMessage(const String& topic, const String& payload);
  void updateState();
  void setState(NodeState state);
  void publishStatus(bool force = false);
  const char* stateName() const;

  PlatformInfo platform_{};
  ConfigStore configStore_{};
  HealthStrip health_{};
  NetworkService network_{};
  MqttService mqtt_{};
  WebService web_{};
  NodeState state_{NodeState::Booting};
  bool started_{false};
  bool timeStarted_{false};
  bool otaInProgress_{false};
  bool faultLatched_{false};
  uint32_t lastStatusMs_{0};
};

}  // namespace stapells

