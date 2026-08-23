#pragma once

#include <Arduino.h>

namespace stapells {

struct CoreConfig {
  String wifiSsid{};
  String wifiPassword{};
  String mqttHost{};
  uint16_t mqttPort{1883};
  String mqttUsername{};
  String mqttPassword{};
  String topicRoot{"Control"};
  String nodeName{};
  int healthLedPin{-1};
  uint8_t healthBrightness{24};

  bool hasWifi() const { return !wifiSsid.isEmpty(); }
  bool hasMqtt() const { return !mqttHost.isEmpty(); }
  bool isComplete() const { return hasWifi() && hasMqtt() && healthLedPin >= 0; }
};

class ConfigStore {
 public:
  bool begin();
  bool load();
  bool save();
  bool updateFromJson(const String& body, String& error);
  bool factoryReset();

  const CoreConfig& config() const { return config_; }
  CoreConfig& config() { return config_; }
  bool mounted() const { return mounted_; }

 private:
  static constexpr const char* kPath = "/stapells-core.json";
  CoreConfig config_{};
  bool mounted_{false};
};

}  // namespace stapells
