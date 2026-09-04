#pragma once

#include <Arduino.h>
#include <PubSubClient.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include "stapells/ConfigStore.h"

namespace stapells {

using MqttMessageHandler = void (*)(const String& topic, const String& payload);

class MqttService {
 public:
  MqttService();
  void begin(const CoreConfig& config, const String& boardId,
             MqttMessageHandler handler);
  void loop(bool networkReady);
  bool connected() { return client_.connected(); }
  bool publish(const String& suffix, const String& payload, bool retained = false);
  bool publishTopic(const String& fullTopic, const String& payload, bool retained = false);
  bool subscribe(const String& fullTopic);
  String topic(const String& suffix) const;
  uint32_t connectionCount() const { return connectionCount_; }

 private:
  static MqttService* instance_;
  static void callback(char* topic, uint8_t* payload, unsigned int length);
  void connect();

  WiFiClient transport_{};
  PubSubClient client_;
  const CoreConfig* config_{nullptr};
  String boardId_{};
  MqttMessageHandler handler_{nullptr};
  uint32_t nextAttemptMs_{0};
  uint32_t connectionCount_{0};
};

}  // namespace stapells
