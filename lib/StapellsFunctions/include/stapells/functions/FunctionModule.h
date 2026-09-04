#pragma once
#include <Arduino.h>
namespace stapells {
class MqttService;
class FunctionModule {
 public:
  virtual ~FunctionModule() = default;
  virtual const char* name() const = 0;
  virtual void begin(MqttService&, const String& boardId) = 0;
  virtual void onConnected() = 0;
  virtual void onMessage(const String& topic, const String& payload) = 0;
  virtual void loop(uint32_t now) = 0;
};
}  // namespace stapells
