#pragma once

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

namespace stapells {

enum class HealthLed : uint8_t { System = 0, Wifi = 1, Mqtt = 2, Activity = 3 };
enum class HealthColour : uint8_t { Off, Green, Blue, Purple, Orange, Yellow, Red };

class HealthStrip {
 public:
  static constexpr uint8_t kLedCount = 4;

  ~HealthStrip();
  void begin(int gpio, uint8_t brightness);
  void set(HealthLed led, HealthColour colour);
  void pulse(HealthLed led, HealthColour colour, uint16_t durationMs = 160);
  void bootWalk();
  void loop();
  bool available() const { return strip_ != nullptr; }

 private:
  uint32_t colour(HealthColour value) const;
  void render();

  Adafruit_NeoPixel* strip_{nullptr};
  HealthColour values_[kLedCount]{HealthColour::Off, HealthColour::Off,
                                  HealthColour::Off, HealthColour::Off};
  bool activityPulse_{false};
  uint32_t activityUntilMs_{0};
};

}  // namespace stapells
