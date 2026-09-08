#include "stapells/MqttService.h"

namespace stapells {
namespace {
constexpr uint32_t kReconnectMs = 5000;
}

MqttService* MqttService::instance_ = nullptr;

MqttService::MqttService() : client_(transport_) {}

void MqttService::begin(const CoreConfig& config, const String& boardId,
                        MqttMessageHandler handler) {
  instance_ = this;
  config_ = &config;
  boardId_ = boardId;
  handler_ = handler;
  client_.setServer(config.mqttHost.c_str(), config.mqttPort);
  client_.setCallback(callback);
  client_.setBufferSize(1024);
  client_.setKeepAlive(30);
}

String MqttService::topic(const String& suffix) const {
  String value = config_ ? config_->topicRoot : "Control";
  value += '/';
  value += boardId_;
  if (!suffix.isEmpty()) {
    value += '/';
    value += suffix;
  }
  return value;
}

void MqttService::loop(bool networkReady) {
  if (!networkReady || !config_ || !config_->hasMqtt()) return;
  if (!client_.connected()) {
    if (static_cast<int32_t>(millis() - nextAttemptMs_) >= 0) connect();
    return;
  }
  client_.loop();
}

void MqttService::connect() {
  nextAttemptMs_ = millis() + kReconnectMs;
  const String clientId = String("stapells-") + boardId_;
  const String willTopic = config_->topicRoot + '/' + boardId_ + "_Status";

  bool ok;
  if (config_->mqttUsername.isEmpty()) {
    ok = client_.connect(clientId.c_str(), willTopic.c_str(), 1, true, "OFFLINE");
  } else {
    ok = client_.connect(clientId.c_str(), config_->mqttUsername.c_str(),
                         config_->mqttPassword.c_str(), willTopic.c_str(), 1,
                         true, "OFFLINE");
  }

  if (!ok) {
    Serial.printf("[mqtt] Connect failed: %d\n", client_.state());
    return;
  }

  Serial.println(F("[mqtt] Connected"));
  ++connectionCount_;
  publishTopic(willTopic, "ONLINE", true);
  const String commands = topic("command/#");
  client_.subscribe(commands.c_str());
}

bool MqttService::publish(const String& suffix, const String& payload, bool retained) {
  if (!client_.connected()) return false;
  const String fullTopic = topic(suffix);
  return client_.publish(fullTopic.c_str(), payload.c_str(), retained);
}

bool MqttService::publishTopic(const String& fullTopic, const String& payload,
                               bool retained) {
  if (!client_.connected()) return false;
  return client_.publish(fullTopic.c_str(), payload.c_str(), retained);
}

bool MqttService::subscribe(const String& fullTopic) {
  if (!client_.connected()) return false;
  return client_.subscribe(fullTopic.c_str());
}

void MqttService::callback(char* incomingTopic, uint8_t* payload, unsigned int length) {
  if (!instance_ || !instance_->handler_) return;
  String body;
  body.reserve(length);
  for (unsigned int i = 0; i < length; ++i) body += static_cast<char>(payload[i]);
  instance_->handler_(String(incomingTopic), body);
}

}  // namespace stapells
