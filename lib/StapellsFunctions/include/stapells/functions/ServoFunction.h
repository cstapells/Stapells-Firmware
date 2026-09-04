#pragma once
#include <Adafruit_PCF8574.h>
#include <Adafruit_PWMServoDriver.h>
#include "stapells/functions/FunctionModule.h"
namespace stapells {
class ServoFunction final : public FunctionModule {
 public:
  const char* name() const override { return "TURNOUT_SERVO"; }
  void begin(MqttService&, const String&) override;
  void onConnected() override;
  void onMessage(const String&, const String&) override;
  void loop(uint32_t) override;
 private:
  static constexpr uint8_t kTurnoutCapacity = 64;
  static constexpr uint8_t kChannelCapacity = 16;
  struct Turnout {
    int id{-1}, channel{-1}, frog{-1};
    uint16_t closed{0}, thrown{0}, current{0}, target{0};
    bool hasClosed{false}, hasThrown{false}, hasState{false}, isThrown{false};
    bool hasBoard{false}, boardMatches{false}, started{false}, moving{false};
    uint32_t nextStep{0};
  };
  Turnout* get(int id);
  bool owns(const Turnout&) const;
  bool listOwns(int id) const;
  void readList(const String&);
  void enableHardware();
  void update(Turnout&);
  void finish(Turnout&);
  void setFrog(int, bool);
  MqttService* mqtt_{nullptr};
  String boardId_;
  Turnout turnouts_[kTurnoutCapacity]{};
  int ownedIds_[kChannelCapacity]{};
  uint8_t ownedCount_{0};
  bool enabled_{false}, hardwareReady_{false}, pcf1Ready_{false}, pcf2Ready_{false};
  Adafruit_PWMServoDriver pwm_{};
  Adafruit_PCF8574 pcf1_{}, pcf2_{};
};
}  // namespace stapells
