#include "StapellsCore.h"

#include <ArduinoJson.h>
#include <time.h>

namespace stapells {
namespace {
constexpr uint32_t kStatusIntervalMs = 30000;
constexpr size_t kMaximumProvisioningLine = 1024;
constexpr char kProvisioningPrefix[] = "STAPELLS_CONFIG ";
}

Core* Core::instance_ = nullptr;

void Core::begin() {
  instance_ = this;
  platform_ = detectPlatform();
  Serial.printf("\n[core] Stapells %s %s on %s (%s)\n", STAPELLS_FIRMWARE_NAME,
                STAPELLS_FIRMWARE_VERSION, platform_.name.c_str(),
                platform_.boardId.c_str());

  if (!configStore_.begin()) Serial.println(F("[core] Starting with defaults"));
  health_.begin(configStore_.config().healthLedPin, configStore_.config().healthBrightness);
  health_.bootWalk();
  setState(NodeState::Booting);

  network_.begin(configStore_.config(), platform_.boardId);
  mqtt_.begin(configStore_.config(), platform_.boardId, onMqttMessage);
  ota_.begin(configStore_.config(), platform_, mqtt_);
  web_.begin(statusJson);
  started_ = true;
  updateState();
  Serial.printf("STAPELLS_READY %s %s\n", platform_.boardId.c_str(),
                STAPELLS_BOARD_TARGET);
}

void Core::loop() {
  if (!started_) return;
  handleSerialProvisioning();
  network_.loop();
  web_.loop();
  mqtt_.loop(network_.stationConnected());
  if (ota_.pending() && network_.stationConnected()) {
    setState(NodeState::Updating);
    ota_.run(true);
  }
  health_.loop();

  if (network_.stationConnected() && !timeStarted_) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    timeStarted_ = true;
  }
  updateState();
  publishStatus();
  platformYield();
}

void Core::handleSerialProvisioning() {
  while (Serial.available()) {
    const char value = static_cast<char>(Serial.read());
    if (value == '\r') continue;
    if (value == '\n') {
      if (serialLine_.startsWith(kProvisioningPrefix)) {
        applySerialConfiguration(serialLine_.substring(strlen(kProvisioningPrefix)));
      }
      serialLine_ = "";
      continue;
    }
    if (value < 0x20 || value > 0x7E) continue;
    if (serialLine_.length() >= kMaximumProvisioningLine) {
      serialLine_ = "";
      Serial.println(F("STAPELLS_CONFIG_ERROR MESSAGE_TOO_LONG"));
      continue;
    }
    serialLine_ += value;
  }
}

void Core::applySerialConfiguration(const String& payload) {
  String error;
  if (!configStore_.updateFromJson(payload, error)) {
    Serial.print(F("STAPELLS_CONFIG_ERROR "));
    Serial.println(error);
    return;
  }
  Serial.print(F("STAPELLS_CONFIG_OK "));
  Serial.println(platform_.boardId);
  Serial.flush();
  delay(300);
  restartPlatform();
}

void Core::updateState() {
  if (faultLatched_) return;
  NodeState next;
  if (!configStore_.config().hasWifi()) next = NodeState::Unconfigured;
  else if (!network_.stationConnected()) next = NodeState::ConnectingWifi;
  else if (!configStore_.config().hasMqtt()) next = NodeState::Unconfigured;
  else if (!mqtt_.connected()) next = NodeState::ConnectingMqtt;
  else if (!configStore_.config().isComplete()) next = NodeState::Unconfigured;
  else next = NodeState::Operational;
  setState(next);
}

void Core::setState(NodeState state) {
  if (state_ == state && started_) return;
  state_ = state;
  health_.set(HealthLed::Activity, HealthColour::Off);
  switch (state_) {
    case NodeState::Booting:
      health_.set(HealthLed::System, HealthColour::Yellow);
      health_.set(HealthLed::Wifi, HealthColour::Off);
      health_.set(HealthLed::Mqtt, HealthColour::Off);
      break;
    case NodeState::ConnectingWifi:
      health_.set(HealthLed::System, HealthColour::Green);
      health_.set(HealthLed::Wifi, HealthColour::Yellow);
      health_.set(HealthLed::Mqtt, HealthColour::Off);
      break;
    case NodeState::ConnectingMqtt:
      health_.set(HealthLed::System, HealthColour::Green);
      health_.set(HealthLed::Wifi, HealthColour::Blue);
      health_.set(HealthLed::Mqtt, HealthColour::Yellow);
      break;
    case NodeState::Unconfigured:
      health_.set(HealthLed::System, HealthColour::Yellow);
      health_.set(HealthLed::Wifi, network_.stationConnected() ? HealthColour::Blue : HealthColour::Off);
      health_.set(HealthLed::Mqtt, mqtt_.connected() ? HealthColour::Purple : HealthColour::Off);
      break;
    case NodeState::Operational:
      health_.set(HealthLed::System, HealthColour::Green);
      health_.set(HealthLed::Wifi, HealthColour::Blue);
      health_.set(HealthLed::Mqtt, HealthColour::Purple);
      break;
    case NodeState::Updating:
    case NodeState::Recovering:
      health_.set(HealthLed::System, HealthColour::Yellow);
      health_.pulse(HealthLed::Activity, HealthColour::Orange, 1000);
      break;
    case NodeState::Fault:
      health_.set(HealthLed::System, HealthColour::Red);
      break;
  }
  Serial.printf("[core] State: %s\n", stateName());
  publishStatus(true);
}

void Core::activity() { health_.pulse(HealthLed::Activity, HealthColour::Orange); }

const char* Core::stateName() const {
  switch (state_) {
    case NodeState::Booting: return "BOOTING";
    case NodeState::ConnectingWifi: return "CONNECTING_WIFI";
    case NodeState::ConnectingMqtt: return "CONNECTING_MQTT";
    case NodeState::Unconfigured: return "UNCONFIGURED";
    case NodeState::Operational: return "OPERATIONAL";
    case NodeState::Updating: return "UPDATING";
    case NodeState::Recovering: return "RECOVERING";
    case NodeState::Fault: return "FAULT";
  }
  return "UNKNOWN";
}

String Core::statusJson() {
  if (!instance_) return "{}";
  JsonDocument doc;
  doc["boardId"] = instance_->platform_.boardId;
  doc["mac"] = instance_->platform_.macAddress;
  doc["platform"] = instance_->platform_.name;
  doc["firmware"] = STAPELLS_FIRMWARE_NAME;
  doc["version"] = STAPELLS_FIRMWARE_VERSION;
  doc["state"] = instance_->stateName();
  doc["ip"] = instance_->network_.ipAddress();
  doc["rssi"] = instance_->network_.rssi();
  doc["mqttConnected"] = instance_->mqtt_.connected();
  doc["configured"] = instance_->configStore_.config().isComplete();
  doc["freeHeap"] = freeHeap();
  doc["uptimeSeconds"] = millis() / 1000UL;
  String output;
  serializeJson(doc, output);
  return output;
}

void Core::publishStatus(bool force) {
  if (!mqtt_.connected()) return;
  const uint32_t now = millis();
  if (!force && now - lastStatusMs_ < kStatusIntervalMs) return;
  lastStatusMs_ = now;
  mqtt_.publish("status/summary", statusJson(), true);
  mqtt_.publish("status/state", stateName(), true);
  mqtt_.publish("identity/platform", platform_.name, true);
  mqtt_.publish("identity/firmware", STAPELLS_FIRMWARE_VERSION, true);
  mqtt_.publishTopic(configStore_.config().topicRoot + "/" + platform_.boardId + "_Version", STAPELLS_FIRMWARE_VERSION, true);
  mqtt_.publishTopic(configStore_.config().topicRoot + "/" + platform_.boardId + "_BoardType", STAPELLS_BOARD_TARGET, true);
  mqtt_.publishTopic(configStore_.config().topicRoot + "/" + platform_.boardId + "_IP", network_.ipAddress(), true);
  mqtt_.publishTopic(configStore_.config().topicRoot + "/" + platform_.boardId + "_OtaCapable", "TRUE", true);
  mqtt_.publish("heartbeat", String(now / 1000UL), false);
}

void Core::onMqttMessage(const String& topic, const String& payload) {
  if (instance_) instance_->handleMqttMessage(topic, payload);
}

void Core::handleMqttMessage(const String& topic, const String& payload) {
  activity();
  if (topic.endsWith("/command/update")) ota_.accept(payload);
  else if (topic.endsWith("/command/reboot")) reboot();
  else if (topic.endsWith("/command/factory-reset") && payload == "CONFIRM") factoryReset();
  else if (topic.endsWith("/command/status")) publishStatus(true);
  if (functionMessageHandler_) functionMessageHandler_(topic, payload);
}

void Core::reboot() {
  delay(100);
  restartPlatform();
}

void Core::factoryReset() {
  if (instance_) instance_->configStore_.factoryReset();
  reboot();
}

}  // namespace stapells

