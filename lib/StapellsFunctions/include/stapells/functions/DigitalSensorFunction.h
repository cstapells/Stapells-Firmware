#pragma once

#include <Adafruit_PCF8575.h>
#include "stapells/functions/FunctionModule.h"

namespace stapells {

class DigitalSensorFunction final : public FunctionModule {
 public:
  const char* name() const override { return "DIGITAL_SENSOR"; }
  void begin(MqttService&, const String&) override;
  void onConnected() override;
  void onMessage(const String&, const String&) override;
  void loop(uint32_t) override;

 private:
  static constexpr uint8_t kChannelCapacity = 32;
  static constexpr uint8_t kExpanderCapacity = 2;
  struct Channel {
    int sensorId{-1};
    bool activeHigh{false};
    uint32_t debounceMs{50};
    bool candidate{false};
    bool stable{false};
    bool initialized{false};
    uint32_t candidateSince{0};
  };

  void readFunctions(const String& payload);
  void readModules(const String& payload);
  void readSensors(const String& payload);
  void readConfig(const String& payload);
  void startHardware();
  void updateStatus();
  void updateChannel(uint8_t channel, uint32_t now);
  void publishState(Channel& channel);

  MqttService* mqtt_{nullptr};
  String boardId_{};
  Channel channels_[kChannelCapacity]{};
  Adafruit_PCF8575 expanders_[kExpanderCapacity]{};
  uint8_t addresses_[kExpanderCapacity]{0x20, 0x21};
  bool ready_[kExpanderCapacity]{false, false};
  uint8_t expanderCount_{1};
  uint8_t mappedCount_{0};
  bool defaultActiveHigh_{false};
  uint32_t defaultDebounceMs_{50};
  bool invertChannel_[kChannelCapacity]{};
  uint32_t debounceOverride_[kChannelCapacity]{};
  bool enabled_{false};
  bool modulesConfigured_{false};
  bool specificSensorsReceived_{false};
  bool specificConfigReceived_{false};
  bool hardwareStarted_{false};
};

}  // namespace stapells
