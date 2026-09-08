#pragma once

#include <BH1750.h>
#include "stapells/functions/FunctionModule.h"

namespace stapells {

class LightSensorFunction final : public FunctionModule {
 public:
  const char* name() const override { return "BH1750_SENSOR"; }
  void begin(MqttService&, const String&) override;
  void onConnected() override;
  void onMessage(const String&, const String&) override;
  void loop(uint32_t) override;

 private:
  struct Config {
    int sensorId{-1};
    uint32_t sampleMs{250};
    uint32_t reportMs{5000};
    uint32_t calibrationMs{15000};
    uint32_t baselineRefreshMs{300000};
    float activeDrop{40.0f};
    float clearDrop{25.0f};
    float learnRate{0.05f};
    float minimumBaseline{1.0f};
  } config_{};

  void readFunctions(const String& payload);
  void readModules(const String& payload);
  void readSensors(const String& payload);
  void readConfig(const String& payload);
  void startHardware();
  void updateStatus();
  void beginCalibration(uint32_t now);
  void sample(uint32_t now);
  void publishState();

  MqttService* mqtt_{nullptr};
  String boardId_{};
  BH1750 meter_{};
  uint8_t address_{0x23};
  bool enabled_{false};
  bool modulesConfigured_{false};
  bool hardwareStarted_{false};
  bool ready_{false};
  bool specificSensorsReceived_{false};
  bool specificConfigReceived_{false};
  bool calibrating_{false};
  bool calibrated_{false};
  bool active_{false};
  bool published_{false};
  float baselineLux_{0.0f};
  float lastLux_{0.0f};
  float calibrationTotal_{0.0f};
  uint32_t calibrationSamples_{0};
  uint32_t calibrationStarted_{0};
  uint32_t lastSample_{0};
  uint32_t lastReport_{0};
  uint32_t lastBaselineRefresh_{0};
};

}  // namespace stapells
