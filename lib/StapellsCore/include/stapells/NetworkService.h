#pragma once

#include <Arduino.h>

#include "stapells/ConfigStore.h"

namespace stapells {

class NetworkService {
 public:
  void begin(const CoreConfig& config, const String& boardId);
  void loop();

  bool stationConnected() const;
  bool setupAccessPointActive() const { return apActive_; }
  String ipAddress() const;
  int32_t rssi() const;

 private:
  void connectStation();
  void startSetupAccessPoint();

  const CoreConfig* config_{nullptr};
  String boardId_{};
  bool apActive_{false};
  uint32_t attemptStartedMs_{0};
  uint32_t nextAttemptMs_{0};
};

}  // namespace stapells
