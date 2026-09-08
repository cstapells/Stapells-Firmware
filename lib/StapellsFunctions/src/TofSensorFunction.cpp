#include "stapells/functions/TofSensorFunction.h"

#include <ArduinoJson.h>
#include <Wire.h>
#include "stapells/MqttService.h"

namespace stapells {
namespace {
constexpr uint32_t kRetryMs = 5000;
constexpr uint16_t kMinimumRangeMm = 1;
constexpr uint16_t kMaximumRangeMm = 1200;

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

bool parseAddress(const String& text, uint8_t minimum, uint8_t maximum, uint8_t& result) {
  char* end = nullptr;
  const long value = strtol(text.c_str(), &end, 0);
  if (end == text.c_str() || *end != '\0' || value < minimum || value > maximum) return false;
  result = static_cast<uint8_t>(value);
  return true;
}

String scanI2c() {
  String found;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() != 0) continue;
    char value[5];
    snprintf(value, sizeof(value), "0x%02X", address);
    if (!found.isEmpty()) found += ',';
    found += value;
  }
  return found.isEmpty() ? "NONE" : found;
}
}

void TofSensorFunction::begin(MqttService& mqtt, const String& boardId) {
  mqtt_ = &mqtt;
  boardId_ = boardId;
  boardId_.toUpperCase();
  legacyTopic_ = String("track/tof/") + boardId_;
}

void TofSensorFunction::onConnected() { mqtt_->subscribe("Control/#"); }

void TofSensorFunction::readFunctions(const String& payload) {
  if (arrayContains(payload, "VL53L0X_SENSOR") || arrayContains(payload, "TOF_SENSOR") ||
      arrayContains(payload, "TOF_DETECTION")) {
    enabled_ = true;
    startHardware(millis());
  }
}

void TofSensorFunction::readModules(const String& payload) {
  JsonDocument document;
  if (deserializeJson(document, payload) || !document.is<JsonArray>()) return;
  JsonArray modules = document.as<JsonArray>();
  muxCount_ = 0;
  for (JsonObject module : modules) {
    String type = module["type"] | "";
    type.toUpperCase();
    if (type != "TCA9548A" || muxCount_ >= kMuxCapacity) continue;
    uint8_t address = 0;
    if (!parseAddress(module["address"] | "0x70", 0x70, 0x77, address)) continue;
    bool duplicate = false;
    for (uint8_t i = 0; i < muxCount_; ++i) duplicate |= muxAddresses_[i] == address;
    if (!duplicate) muxAddresses_[muxCount_++] = address;
  }

  sensorCount_ = 0;
  for (uint8_t i = 0; i < kSensorCapacity; ++i) {
    channels_[i].ready = false;
    channels_[i].hasPublishedState = false;
  }
  for (JsonObject module : modules) {
    if (sensorCount_ >= kSensorCapacity) break;
    String type = module["type"] | "";
    type.toUpperCase();
    if (type != "VL53L0X" && type != "TOF") continue;
    uint8_t address = 0;
    if (!parseAddress(module["address"] | "0x29", 0x29, 0x29, address)) {
      Serial.println(F("[tof] VL53L0X address must be 0x29"));
      continue;
    }
    SensorChannel& channel = channels_[sensorCount_];
    channel.address = address;
    channel.throughMux = !module["mux_channel"].isNull();
    if (channel.throughMux) {
      uint8_t muxAddress = 0;
      const int muxChannel = module["mux_channel"].as<int>();
      if (!parseAddress(module["mux_address"] | "0x70", 0x70, 0x77, muxAddress) ||
          muxChannel < 0 || muxChannel > 7) {
        Serial.println(F("[tof] Invalid TCA9548A route ignored"));
        continue;
      }
      bool configuredMux = false;
      for (uint8_t i = 0; i < muxCount_; ++i) configuredMux |= muxAddresses_[i] == muxAddress;
      if (!configuredMux) {
        Serial.println(F("[tof] Routed sensor requires a matching TCA9548A module"));
        continue;
      }
      channel.muxAddress = muxAddress;
      channel.muxChannel = static_cast<uint8_t>(muxChannel);
    }
    bool duplicateRoute = false;
    for (uint8_t i = 0; i < sensorCount_; ++i) {
      const SensorChannel& prior = channels_[i];
      duplicateRoute |= channel.throughMux == prior.throughMux &&
                        (!channel.throughMux || (channel.muxAddress == prior.muxAddress &&
                                                 channel.muxChannel == prior.muxChannel));
    }
    if (duplicateRoute) {
      Serial.println(F("[tof] Duplicate VL53L0X route ignored"));
      continue;
    }
    ++sensorCount_;
  }
  modulesConfigured_ = sensorCount_ > 0;
  if (modulesConfigured_) {
    enabled_ = true;
    startHardware(millis());
  }
}

void TofSensorFunction::readSensors(const String& payload) {
  for (uint8_t i = 0; i < kSensorCapacity; ++i) {
    channels_[i].sensorId = -1;
    channels_[i].hasPublishedState = false;
  }
  int start = 0;
  for (uint8_t index = 0; index < kSensorCapacity; ++index) {
    if (start > static_cast<int>(payload.length())) break;
    int comma = payload.indexOf(',', start);
    if (comma < 0) comma = payload.length();
    String item = payload.substring(start, comma);
    item.trim();
    if (validSensorId(item)) {
      const int sensorId = item.toInt();
      bool duplicate = false;
      for (uint8_t prior = 0; prior < index; ++prior) duplicate |= channels_[prior].sensorId == sensorId;
      if (!duplicate) channels_[index].sensorId = sensorId;
    }
    if (comma == static_cast<int>(payload.length())) break;
    start = comma + 1;
  }
  updateStatus();
}

void TofSensorFunction::readConfig(const String& payload) {
  JsonDocument document;
  if (deserializeJson(document, payload) || !document.is<JsonObject>()) {
    Serial.println(F("[tof] Invalid sensor configuration JSON"));
    return;
  }
  JsonObject object = document.as<JsonObject>();
  if (!object["read_interval_ms"].isNull()) {
    const long value = object["read_interval_ms"].as<long>();
    if (value >= 25 && value <= 60000) config_.readIntervalMs = value;
  }
  if (!object["minimum_mm"].isNull()) {
    const long value = object["minimum_mm"].as<long>();
    if (value >= kMinimumRangeMm && value <= kMaximumRangeMm) config_.minimumMm = value;
  }
  if (!object["maximum_mm"].isNull()) {
    const long value = object["maximum_mm"].as<long>();
    if (value >= kMinimumRangeMm && value <= kMaximumRangeMm) config_.maximumMm = value;
  }
  if (!validWindow()) {
    Serial.println(F("[tof] minimum_mm exceeds maximum_mm; detector held INACTIVE"));
    for (uint8_t index = 0; index < sensorCount_; ++index) {
      publishState(index, false, true);
    }
  }
  publishLegacyWindow();
  updateStatus();
}

void TofSensorFunction::startHardware(uint32_t now) {
  if (!enabled_ || !modulesConfigured_) return;
  if (!wireStarted_) {
    Wire.begin();
    wireStarted_ = true;
  }
  lastRetry_ = now;
  disableMuxes();
  mqtt_->publishTopic(String("Control/") + boardId_ + "_I2CScan", scanI2c(), true);
  uint8_t readyCount = 0;
  for (uint8_t index = 0; index < sensorCount_; ++index) {
    SensorChannel& channel = channels_[index];
    if (!channel.ready && selectRoute(channel)) channel.ready = sensors_[index].begin(channel.address, false, &Wire);
    if (channel.ready) ++readyCount;
    Serial.printf("[tof] VL53L0X %u at 0x%02X%s: %s\n", index + 1, channel.address,
                  channel.throughMux ? " through TCA9548A" : " direct",
                  channel.ready ? "ready" : "missing");
    if (!channel.ready) {
      channel.lastDistanceMm = -1;
      publishState(index, false, true);
      if (channel.sensorId >= 0) {
        mqtt_->publishTopic(String("track/sensors/") + channel.sensorId + "/value", "-1", true);
        mqtt_->publishTopic(String("track/sensors/") + channel.sensorId + "/raw", "TOF_MISSING", true);
      }
      if (legacyPublishing_ && index == 0) {
        mqtt_->publishTopic(legacyTopic_ + "/mm", "-1", true);
        mqtt_->publishTopic(legacyTopic_ + "/raw", "TOF_MISSING", true);
      }
    }
  }
  disableMuxes();
  mqtt_->publishTopic(String("Control/") + boardId_ + "_TofStatus",
                      readyCount == sensorCount_ ? "FOUND" : readyCount ? "PARTIAL" : "MISSING", true);
  updateStatus();
}

void TofSensorFunction::disableMuxes() {
  for (uint8_t i = 0; i < muxCount_; ++i) {
    Wire.beginTransmission(muxAddresses_[i]);
    Wire.write(0);
    Wire.endTransmission();
  }
}

bool TofSensorFunction::selectRoute(const SensorChannel& channel) {
  disableMuxes();
  if (!channel.throughMux) return true;
  Wire.beginTransmission(channel.muxAddress);
  Wire.write(static_cast<uint8_t>(1U << channel.muxChannel));
  return Wire.endTransmission() == 0;
}

void TofSensorFunction::updateStatus() {
  if (!mqtt_ || !enabled_) return;
  String status = "READY";
  if (!validWindow()) status = "FAULT_CONFIG";
  else if (!modulesConfigured_) status = "FAULT_HARDWARE";
  else {
    uint8_t mapped = 0;
    for (uint8_t i = 0; i < sensorCount_; ++i) {
      if (!channels_[i].ready) status = "FAULT_HARDWARE";
      if (channels_[i].sensorId >= 0) ++mapped;
    }
    if (status == "READY" && mapped == 0 && !legacyPublishing_) status = "NOT_CONFIGURED";
  }
  mqtt_->publishTopic(String("Control/") + boardId_ +
                          "/status/functions/tof-sensor",
                      status, true);
}

void TofSensorFunction::publishState(uint8_t index, bool active, bool force) {
  SensorChannel& channel = channels_[index];
  const bool changed = !channel.hasPublishedState || active != channel.active;
  channel.active = active;
  if (!force && !changed) return;
  const String payload = active ? "ACTIVE" : "INACTIVE";
  if (channel.sensorId >= 0) {
    mqtt_->publishTopic(String("track/sensors/") + channel.sensorId, payload, true);
  }
  if (legacyPublishing_ && index == 0) mqtt_->publishTopic(legacyTopic_, payload, true);
  channel.hasPublishedState = true;
}

void TofSensorFunction::publishLegacyWindow() {
  if (!legacyPublishing_) return;
  mqtt_->publishTopic(legacyTopic_ + "/window",
                      String(config_.minimumMm) + '-' + config_.maximumMm, true);
}

void TofSensorFunction::readDistance(uint8_t index) {
  SensorChannel& channel = channels_[index];
  if (channel.sensorId < 0 && !(legacyPublishing_ && index == 0)) return;
  if (!channel.ready || !selectRoute(channel)) {
    channel.ready = false;
    publishState(index, false);
    return;
  }
  VL53L0X_RangingMeasurementData_t measurement;
  sensors_[index].rangingTest(&measurement, false);
  const bool valid = measurement.RangeStatus != 4 &&
                     measurement.RangeMilliMeter >= kMinimumRangeMm &&
                     measurement.RangeMilliMeter <= kMaximumRangeMm;
  if (valid) {
    channel.lastDistanceMm = measurement.RangeMilliMeter;
    const bool inside = validWindow() && channel.lastDistanceMm >= config_.minimumMm &&
                        channel.lastDistanceMm <= config_.maximumMm;
    publishState(index, inside);
    if (channel.sensorId >= 0) {
      mqtt_->publishTopic(String("track/sensors/") + channel.sensorId + "/value",
                          String(channel.lastDistanceMm), true);
      mqtt_->publishTopic(String("track/sensors/") + channel.sensorId + "/raw", "VALID", true);
    }
    if (legacyPublishing_ && index == 0) {
      mqtt_->publishTopic(legacyTopic_ + "/mm", String(channel.lastDistanceMm), true);
      mqtt_->publishTopic(legacyTopic_ + "/raw", "VALID", true);
    }
  } else {
    channel.lastDistanceMm = -1;
    publishState(index, false);
    if (channel.sensorId >= 0) {
      mqtt_->publishTopic(String("track/sensors/") + channel.sensorId + "/value", "-1", true);
      mqtt_->publishTopic(String("track/sensors/") + channel.sensorId + "/raw", "OUT_OF_RANGE", true);
    }
    if (legacyPublishing_ && index == 0) {
      mqtt_->publishTopic(legacyTopic_ + "/mm", "-1", true);
      mqtt_->publishTopic(legacyTopic_ + "/raw", "OUT_OF_RANGE", true);
    }
  }
}

void TofSensorFunction::onMessage(const String& topic, const String& payload) {
  const String control = String("Control/") + boardId_ + '_';
  if (topic == control + "Type") {
    String value = payload;
    value.trim();
    value.toUpperCase();
    if (value == "TOF_SPEED") {
      enabled_ = true;
      if (sensorCount_ == 0) {
        sensorCount_ = 1;
        channels_[0].address = 0x29;
        channels_[0].throughMux = false;
      }
      modulesConfigured_ = true;
      legacyPublishing_ = true;
      startHardware(millis());
    }
  } else if (topic == control + "SpeedTopic") {
    String value = payload;
    value.trim();
    if (!value.isEmpty()) legacyTopic_ = value;
    legacyPublishing_ = true;
    if (sensorCount_ > 0) publishState(0, channels_[0].active, true);
    publishLegacyWindow();
  } else if (topic == control + "ReadIntervalMS") {
    const long value = payload.toInt();
    if (value >= 25 && value <= 60000) config_.readIntervalMs = value;
  } else if (topic == control + "TofMinMM") {
    const long value = payload.toInt();
    if (value >= kMinimumRangeMm && value <= kMaximumRangeMm) config_.minimumMm = value;
    publishLegacyWindow();
  } else if (topic == control + "TofThresholdMM") {
    const long value = payload.toInt();
    if (value >= kMinimumRangeMm && value <= kMaximumRangeMm) config_.maximumMm = value;
    publishLegacyWindow();
  } else if (topic == control + "Functions") readFunctions(payload);
  else if (topic == control + "Modules") readModules(payload);
  else if (topic == control + "ToFSensors") {
    specificSensorsReceived_ = true;
    readSensors(payload);
  } else if (topic == control + "Sensors" && !specificSensorsReceived_) {
    readSensors(payload);
  } else if (topic == control + "ToFConfig") {
    specificConfigReceived_ = true;
    readConfig(payload);
  } else if (topic == control + "SensorConfig" && !specificConfigReceived_) {
    readConfig(payload);
  }
}

void TofSensorFunction::loop(uint32_t now) {
  if (!enabled_ || !modulesConfigured_) return;
  bool missing = false;
  for (uint8_t i = 0; i < sensorCount_; ++i) missing |= !channels_[i].ready;
  if (missing && now - lastRetry_ >= kRetryMs) startHardware(now);
  if (now - lastRead_ < config_.readIntervalMs) return;
  lastRead_ = now;
  for (uint8_t i = 0; i < sensorCount_; ++i) readDistance(i);
  disableMuxes();
}

}  // namespace stapells
