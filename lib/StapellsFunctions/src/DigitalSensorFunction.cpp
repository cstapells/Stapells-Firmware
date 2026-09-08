#include "stapells/functions/DigitalSensorFunction.h"

#include <ArduinoJson.h>
#include <Wire.h>
#include "stapells/MqttService.h"

namespace stapells {
namespace {
bool arrayContains(const String& payload, const char* wanted) {
  JsonDocument document;
  if (deserializeJson(document, payload) || !document.is<JsonArray>()) return false;
  for (JsonVariant value : document.as<JsonArray>()) {
    String name = value.as<String>();
    name.toUpperCase();
    if (name == wanted) return true;
  }
  return false;
}

bool validSensorId(const String& value) {
  if (value.isEmpty()) return false;
  for (size_t i = 0; i < value.length(); ++i) if (!isDigit(value[i])) return false;
  return true;
}
}

void DigitalSensorFunction::begin(MqttService& mqtt, const String& boardId) {
  mqtt_ = &mqtt;
  boardId_ = boardId;
  boardId_.toUpperCase();
  for (auto& value : debounceOverride_) value = UINT32_MAX;
}

void DigitalSensorFunction::onConnected() { mqtt_->subscribe("Control/#"); }

void DigitalSensorFunction::readFunctions(const String& payload) {
  if (arrayContains(payload, "DIGITAL_SENSOR") || arrayContains(payload, "BLOCK_DETECTION") ||
      arrayContains(payload, "PCF8575")) {
    enabled_ = true;
    startHardware();
  }
}

void DigitalSensorFunction::readModules(const String& payload) {
  JsonDocument document;
  if (deserializeJson(document, payload) || !document.is<JsonArray>()) {
    Serial.println(F("[digital-sensor] Invalid _Modules JSON"));
    return;
  }
  uint8_t count = 0;
  for (JsonObject module : document.as<JsonArray>()) {
    if (count >= kExpanderCapacity) break;
    String type = module["type"] | "";
    type.toUpperCase();
    if (type != "PCF8575") continue;
    String addressText = module["address"] | "0x20";
    char* end = nullptr;
    const long address = strtol(addressText.c_str(), &end, 0);
    if (end == addressText.c_str() || *end != '\0' || address < 0x20 || address > 0x27) continue;
    bool duplicate = false;
    for (uint8_t i = 0; i < count; ++i) duplicate |= addresses_[i] == address;
    if (!duplicate) addresses_[count++] = static_cast<uint8_t>(address);
  }
  if (count > 0) {
    expanderCount_ = count;
    modulesConfigured_ = true;
    enabled_ = true;
    startHardware();
  }
}

void DigitalSensorFunction::readSensors(const String& payload) {
  mappedCount_ = 0;
  for (uint8_t i = 0; i < kChannelCapacity; ++i) {
    channels_[i] = Channel{};
    channels_[i].activeHigh = defaultActiveHigh_ != invertChannel_[i];
    channels_[i].debounceMs = debounceOverride_[i] == UINT32_MAX
                                  ? defaultDebounceMs_ : debounceOverride_[i];
  }
  int start = 0;
  for (uint8_t channel = 0; channel < kChannelCapacity; ++channel) {
    if (start > static_cast<int>(payload.length())) break;
    int comma = payload.indexOf(',', start);
    if (comma < 0) comma = payload.length();
    String item = payload.substring(start, comma);
    item.trim();
    if (validSensorId(item)) {
      const int sensorId = item.toInt();
      bool duplicate = false;
      for (uint8_t prior = 0; prior < channel; ++prior) {
        duplicate |= channels_[prior].sensorId == sensorId;
      }
      if (!duplicate) {
        channels_[channel].sensorId = sensorId;
        ++mappedCount_;
      } else {
        Serial.printf("[digital-sensor] Duplicate sensor ID %d ignored on channel %u\n",
                      sensorId, channel);
      }
    }
    if (comma == static_cast<int>(payload.length())) break;
    start = comma + 1;
  }
  Serial.printf("[digital-sensor] Mapped %u channel(s)\n", mappedCount_);
  updateStatus();
}

void DigitalSensorFunction::readConfig(const String& payload) {
  JsonDocument document;
  if (deserializeJson(document, payload) || !document.is<JsonObject>()) {
    Serial.println(F("[digital-sensor] Invalid _SensorConfig JSON"));
    return;
  }
  String polarity = document["polarity"] | "ACTIVE_LOW";
  polarity.toUpperCase();
  defaultActiveHigh_ = polarity == "ACTIVE_HIGH";
  const int requestedDebounce = document["debounce_ms"] | 50;
  defaultDebounceMs_ = static_cast<uint32_t>(constrain(requestedDebounce, 0, 5000));
  for (uint8_t i = 0; i < kChannelCapacity; ++i) {
    invertChannel_[i] = false;
    debounceOverride_[i] = UINT32_MAX;
  }
  for (JsonVariant item : document["invert_channels"].as<JsonArray>()) {
    const int channel = item.as<int>();
    if (channel >= 0 && channel < kChannelCapacity) invertChannel_[channel] = true;
  }
  for (JsonPair item : document["debounce_overrides"].as<JsonObject>()) {
    const int channel = atoi(item.key().c_str());
    const int value = item.value().as<int>();
    if (channel >= 0 && channel < kChannelCapacity && value >= 0 && value <= 5000) {
      debounceOverride_[channel] = value;
    }
  }
  for (uint8_t i = 0; i < kChannelCapacity; ++i) {
    channels_[i].activeHigh = defaultActiveHigh_ != invertChannel_[i];
    channels_[i].debounceMs = debounceOverride_[i] == UINT32_MAX
                                  ? defaultDebounceMs_ : debounceOverride_[i];
    channels_[i].initialized = false;
  }
}

void DigitalSensorFunction::startHardware() {
  if (!enabled_ || !modulesConfigured_) return;
  if (!hardwareStarted_) {
    Wire.begin();
    hardwareStarted_ = true;
  }
  for (uint8_t board = 0; board < kExpanderCapacity; ++board) ready_[board] = false;
  for (uint8_t board = 0; board < expanderCount_; ++board) {
    ready_[board] = expanders_[board].begin(addresses_[board], &Wire);
    if (ready_[board]) {
      for (uint8_t pin = 0; pin < 16; ++pin) expanders_[board].pinMode(pin, INPUT_PULLUP);
    }
    Serial.printf("[digital-sensor] PCF8575 %u at 0x%02X: %s\n", board + 1,
                  addresses_[board], ready_[board] ? "ready" : "missing");
  }
  updateStatus();
}

void DigitalSensorFunction::updateStatus() {
  if (!mqtt_ || !enabled_) return;
  String status = "READY";
  if (mappedCount_ == 0) status = "NOT_CONFIGURED";
  for (uint8_t board = 0; board < expanderCount_; ++board) {
    if (!ready_[board]) status = "FAULT_HARDWARE";
  }
  mqtt_->publishTopic(String("Control/") + boardId_ +
                          "/status/functions/digital-sensor",
                      status, true);
}

void DigitalSensorFunction::onMessage(const String& topic, const String& payload) {
  const String control = String("Control/") + boardId_ + '_';
  if (topic == control + "Functions") readFunctions(payload);
  else if (topic == control + "Modules") readModules(payload);
  else if (topic == control + "DigitalSensors") {
    specificSensorsReceived_ = true;
    readSensors(payload);
  } else if (topic == control + "Sensors" && !specificSensorsReceived_) {
    readSensors(payload);
  } else if (topic == control + "DigitalSensorConfig") {
    specificConfigReceived_ = true;
    readConfig(payload);
  } else if (topic == control + "SensorConfig" && !specificConfigReceived_) {
    readConfig(payload);
  }
}

void DigitalSensorFunction::publishState(Channel& channel) {
  if (channel.sensorId < 0) return;
  mqtt_->publishTopic(String("track/sensors/") + channel.sensorId,
                      channel.stable ? "ACTIVE" : "INACTIVE", true);
}

void DigitalSensorFunction::updateChannel(uint8_t channelIndex, uint32_t now) {
  Channel& channel = channels_[channelIndex];
  if (channel.sensorId < 0) return;
  const uint8_t board = channelIndex / 16;
  if (board >= expanderCount_ || !ready_[board]) return;
  const bool raw = expanders_[board].digitalRead(channelIndex % 16);
  const bool active = channel.activeHigh ? raw : !raw;
  if (!channel.initialized) {
    channel.candidate = active;
    channel.stable = active;
    channel.candidateSince = now;
    channel.initialized = true;
    publishState(channel);
    return;
  }
  if (active != channel.candidate) {
    channel.candidate = active;
    channel.candidateSince = now;
  }
  if (channel.candidate != channel.stable && now - channel.candidateSince >= channel.debounceMs) {
    channel.stable = channel.candidate;
    publishState(channel);
  }
}

void DigitalSensorFunction::loop(uint32_t now) {
  if (!enabled_ || !hardwareStarted_ || mappedCount_ == 0) return;
  for (uint8_t channel = 0; channel < kChannelCapacity; ++channel) updateChannel(channel, now);
}

}  // namespace stapells
