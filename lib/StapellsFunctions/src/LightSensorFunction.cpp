#include "stapells/functions/LightSensorFunction.h"

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

int firstSensorId(const String& payload) {
  const int comma = payload.indexOf(',');
  String item = comma < 0 ? payload : payload.substring(0, comma);
  item.trim();
  if (item.isEmpty()) return -1;
  for (size_t i = 0; i < item.length(); ++i) if (!isDigit(item[i])) return -1;
  return item.toInt();
}

void assignInteger(JsonObject object, const char* key, uint32_t minimum,
                   uint32_t maximum, uint32_t& target) {
  if (object[key].isNull()) return;
  const long value = object[key].as<long>();
  if (value >= static_cast<long>(minimum) && value <= static_cast<long>(maximum)) {
    target = static_cast<uint32_t>(value);
  }
}

void assignFloat(JsonObject object, const char* key, float minimum, float maximum, float& target) {
  if (object[key].isNull()) return;
  const float value = object[key].as<float>();
  if (isfinite(value) && value >= minimum && value <= maximum) target = value;
}
}

void LightSensorFunction::begin(MqttService& mqtt, const String& boardId) {
  mqtt_ = &mqtt;
  boardId_ = boardId;
  boardId_.toUpperCase();
}

void LightSensorFunction::onConnected() { mqtt_->subscribe("Control/#"); }

void LightSensorFunction::readFunctions(const String& payload) {
  if (arrayContains(payload, "BH1750_SENSOR") || arrayContains(payload, "BH1750")) {
    enabled_ = true;
    startHardware();
  }
}

void LightSensorFunction::readModules(const String& payload) {
  JsonDocument document;
  if (deserializeJson(document, payload) || !document.is<JsonArray>()) return;
  for (JsonObject module : document.as<JsonArray>()) {
    String type = module["type"] | "";
    type.toUpperCase();
    if (type != "BH1750") continue;
    String addressText = module["address"] | "0x23";
    char* end = nullptr;
    const long address = strtol(addressText.c_str(), &end, 0);
    if (end != addressText.c_str() && *end == '\0' && (address == 0x23 || address == 0x5C)) {
      address_ = static_cast<uint8_t>(address);
      modulesConfigured_ = true;
      enabled_ = true;
      startHardware();
    }
    return;
  }
}

void LightSensorFunction::readSensors(const String& payload) {
  config_.sensorId = firstSensorId(payload);
  published_ = false;
  updateStatus();
}

void LightSensorFunction::readConfig(const String& payload) {
  JsonDocument document;
  if (deserializeJson(document, payload) || !document.is<JsonObject>()) {
    Serial.println(F("[bh1750] Invalid sensor configuration JSON"));
    return;
  }
  JsonObject object = document.as<JsonObject>();
  assignInteger(object, "sample_ms", 100UL, 60000UL, config_.sampleMs);
  assignInteger(object, "report_ms", 500UL, 3600000UL, config_.reportMs);
  assignInteger(object, "calibration_ms", 1000UL, 300000UL, config_.calibrationMs);
  assignInteger(object, "baseline_refresh_ms", 1000UL, 86400000UL, config_.baselineRefreshMs);
  assignFloat(object, "active_drop", 1.0f, 99.0f, config_.activeDrop);
  assignFloat(object, "clear_drop", 0.0f, 98.0f, config_.clearDrop);
  assignFloat(object, "learn_rate", 0.001f, 1.0f, config_.learnRate);
  assignFloat(object, "minimum_lux", 0.01f, 10000.0f, config_.minimumBaseline);
  if (config_.clearDrop >= config_.activeDrop) {
    Serial.println(F("[bh1750] clear_drop must be lower than active_drop; keeping safe hysteresis"));
    config_.clearDrop = config_.activeDrop * 0.5f;
  }
}

void LightSensorFunction::startHardware() {
  if (!enabled_ || !modulesConfigured_ || hardwareStarted_) return;
  Wire.begin();
  hardwareStarted_ = true;
  ready_ = meter_.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, address_, &Wire);
  Serial.printf("[bh1750] Sensor at 0x%02X: %s\n", address_, ready_ ? "ready" : "missing");
  if (ready_) beginCalibration(millis());
  updateStatus();
}

void LightSensorFunction::updateStatus() {
  if (!mqtt_ || !enabled_) return;
  String status = "READY";
  if (!ready_) status = "FAULT_HARDWARE";
  else if (config_.sensorId < 0) status = "NOT_CONFIGURED";
  mqtt_->publishTopic(String("Control/") + boardId_ +
                          "/status/functions/bh1750-sensor",
                      status, true);
}

void LightSensorFunction::beginCalibration(uint32_t now) {
  calibrationTotal_ = 0.0f;
  calibrationSamples_ = 0;
  calibrationStarted_ = now;
  lastSample_ = now - config_.sampleMs;
  calibrating_ = true;
  calibrated_ = false;
  published_ = false;
}

void LightSensorFunction::onMessage(const String& topic, const String& payload) {
  const String control = String("Control/") + boardId_ + '_';
  if (topic == control + "Functions") readFunctions(payload);
  else if (topic == control + "Modules") readModules(payload);
  else if (topic == control + "BH1750Sensors") {
    specificSensorsReceived_ = true;
    readSensors(payload);
  } else if (topic == control + "Sensors" && !specificSensorsReceived_) {
    readSensors(payload);
  } else if (topic == control + "BH1750Config") {
    specificConfigReceived_ = true;
    readConfig(payload);
  } else if (topic == control + "SensorConfig" && !specificConfigReceived_) {
    readConfig(payload);
  } else if (topic == control + "Recalibrate") {
    String value = payload;
    value.trim();
    value.toUpperCase();
    if (value == "TRUE" && ready_) beginCalibration(millis());
  }
}

void LightSensorFunction::publishState() {
  if (config_.sensorId < 0) return;
  mqtt_->publishTopic(String("track/sensors/") + config_.sensorId,
                      active_ ? "ACTIVE" : "INACTIVE", true);
  published_ = true;
}

void LightSensorFunction::sample(uint32_t now) {
  const float lux = meter_.readLightLevel();
  if (lux < 0.0f) return;
  lastLux_ = lux;
  if (calibrating_) {
    calibrationTotal_ += lux;
    ++calibrationSamples_;
    if (now - calibrationStarted_ >= config_.calibrationMs) {
      baselineLux_ = calibrationSamples_ ? calibrationTotal_ / calibrationSamples_
                                        : config_.minimumBaseline;
      if (baselineLux_ < config_.minimumBaseline) baselineLux_ = config_.minimumBaseline;
      calibrating_ = false;
      calibrated_ = true;
      active_ = false;
      lastBaselineRefresh_ = now;
      publishState();
    }
    return;
  }
  if (!calibrated_) return;
  const float activeThreshold = baselineLux_ * (1.0f - config_.activeDrop / 100.0f);
  const float clearThreshold = baselineLux_ * (1.0f - config_.clearDrop / 100.0f);
  bool next = active_;
  if (!active_ && lux < activeThreshold) next = true;
  else if (active_ && lux > clearThreshold) next = false;
  if (!published_ || next != active_) {
    active_ = next;
    publishState();
  }
  if (!active_ && now - lastBaselineRefresh_ >= config_.baselineRefreshMs) {
    baselineLux_ = baselineLux_ * (1.0f - config_.learnRate) + lux * config_.learnRate;
    if (baselineLux_ < config_.minimumBaseline) baselineLux_ = config_.minimumBaseline;
    lastBaselineRefresh_ = now;
  }
  if (now - lastReport_ >= config_.reportMs) {
    mqtt_->publishTopic(String("track/sensors/") + config_.sensorId + "/value",
                        String(lux, 2), true);
    lastReport_ = now;
  }
}

void LightSensorFunction::loop(uint32_t now) {
  if (!enabled_ || !ready_ || config_.sensorId < 0) return;
  if (now - lastSample_ < config_.sampleMs) return;
  lastSample_ = now;
  sample(now);
}

}  // namespace stapells
