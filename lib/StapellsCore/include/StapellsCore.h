#pragma once

#include <Arduino.h>

#include "stapells/ConfigStore.h"
#include "stapells/HealthStrip.h"
#include "stapells/MqttService.h"
#include "stapells/NetworkService.h"
#include "stapells/OtaService.h"
#include "stapells/Platform.h"
#include "stapells/WebService.h"

namespace stapells {

using FunctionMessageHandler = void (*)(const String&, const String&);

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
  MqttService& mqtt() { return mqtt_; }
  void setFunctionMessageHandler(FunctionMessageHandler handler) { functionMessageHandler_ = handler; }

 private:
  static Core* instance_;
  static void onMqttMessage(const String& topic, const String& payload);
  static String statusJson();
  static void reboot();
  static void factoryReset();

  void handleMqttMessage(const String& topic, const String& payload);
  void handleSerialProvisioning();
  void applySerialConfiguration(const String& payload);
  void updateState();
  void setState(NodeState state);
  void publishStatus(bool force = false);
  const char* stateName() const;

  PlatformInfo platform_{};
  ConfigStore configStore_{};
  HealthStrip health_{};
  NetworkService network_{};
  MqttService mqtt_{};
  OtaService ota_{};
  WebService web_{};
  NodeState state_{NodeState::Booting};
  bool started_{false};
  bool timeStarted_{false};
  bool faultLatched_{false};
  uint32_t lastStatusMs_{0};
  String serialLine_{};
  FunctionMessageHandler functionMessageHandler_{nullptr};
};

}  // namespace stapells

