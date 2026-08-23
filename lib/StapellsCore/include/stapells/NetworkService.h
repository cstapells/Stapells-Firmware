#pragma once

#include <Arduino.h>

#include "stapells/ConfigStore.h"

namespace stapells {

class NetworkService {
 public:
  void begin(const CoreConfig& config, const String& boardId);
  void loop();

  bool stationConnected() const;
  String ipAddress() const;
  int32_t rssi() const;

 private:
  void connectStation();
  const CoreConfig* config_{nullptr};
  String boardId_{};
  uint32_t attemptStartedMs_{0};
  uint32_t nextAttemptMs_{0};
};

}  // namespace stapells

