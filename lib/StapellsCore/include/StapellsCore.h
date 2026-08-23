#pragma once

#include <Arduino.h>

#include "stapells/HealthStrip.h"
#include "stapells/Platform.h"

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
  void setState(NodeState state);
  NodeState state() const { return state_; }

  const PlatformInfo& platform() const { return platform_; }

 private:
  PlatformInfo platform_{};
  HealthStrip health_{};
  NodeState state_{NodeState::Booting};
};

}  // namespace stapells
