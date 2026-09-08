#pragma once

#include <Adafruit_VL53L0X.h>
#include "stapells/functions/FunctionModule.h"

namespace stapells {

class TofSensorFunction final : public FunctionModule {
 public:
  const char* name() const override { return "VL53L0X_SENSOR"; }
  void begin(MqttService&, const String&) override;
  void onConnected() override;
  void onMessage(const String&, const String&) override;
  void loop(uint32_t) override;

 private:
  static constexpr uint8_t kSensorCapacity = 8;
  static constexpr uint8_t kMuxCapacity = 8;
  struct Config {
    uint32_t readIntervalMs{1000};
    uint16_t minimumMm{10};
    uint16_t maximumMm{80};
  } config_{};

  struct SensorChannel {
    int sensorId{-1};
    uint8_t address{0x29};
    bool throughMux{false};
    uint8_t muxAddress{0x70};
    uint8_t muxChannel{0};
    bool ready{false};
    bool active{false};
    bool hasPublishedState{false};
    int lastDistanceMm{-1};
  } channels_[kSensorCapacity]{};

  void readFunctions(const String& payload);
  void readModules(const String& payload);
  void readSensors(const String& payload);
  void readConfig(const String& payload);
  void startHardware(uint32_t now);
  void updateStatus();
  void readDistance(uint8_t index);
  void publishState(uint8_t index, bool active, bool force = false);
  void publishLegacyWindow();
  bool selectRoute(const SensorChannel& channel);
  void disableMuxes();
  bool validWindow() const { return config_.minimumMm <= config_.maximumMm; }

  MqttService* mqtt_{nullptr};
  String boardId_{};
  String legacyTopic_{};
  Adafruit_VL53L0X sensors_[kSensorCapacity]{};
  uint8_t sensorCount_{0};
  uint8_t muxAddresses_[kMuxCapacity]{};
  uint8_t muxCount_{0};
  bool enabled_{false};
  bool modulesConfigured_{false};
  bool wireStarted_{false};
  bool legacyPublishing_{false};
  bool specificSensorsReceived_{false};
  bool specificConfigReceived_{false};
  uint32_t lastRead_{0};
  uint32_t lastRetry_{0};
};

}  // namespace stapells
