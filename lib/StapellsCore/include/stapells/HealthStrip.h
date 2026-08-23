#pragma once

#include <Arduino.h>

namespace stapells {

enum class HealthLed : uint8_t {
  System = 0,
  Wifi = 1,
  Mqtt = 2,
  Activity = 3
};

enum class HealthColour : uint8_t {
  Off,
  Green,
  Blue,
  Purple,
  Orange,
  Yellow,
  Red
};

class HealthStrip {
 public:
  static constexpr uint8_t kLedCount = 4;

  void begin(int gpio = -1);
  void set(HealthLed led, HealthColour colour);
  void pulse(HealthLed led, HealthColour colour);
  void bootWalk();
  void loop();

  int gpio() const { return gpio_; }

 private:
  int gpio_{-1};
};

}  // namespace stapells
