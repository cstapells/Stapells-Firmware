#include <StapellsFunctions.h>
namespace stapells {
FunctionRuntime* FunctionRuntime::instance_ = nullptr;
void FunctionRuntime::begin(Core& core) {
  instance_ = this; core_ = &core;
  servo_.begin(core.mqtt(), core.platform().boardId);
  digitalSensor_.begin(core.mqtt(), core.platform().boardId);
  lightSensor_.begin(core.mqtt(), core.platform().boardId);
  tofSensor_.begin(core.mqtt(), core.platform().boardId);
  core.setFunctionMessageHandler(onMessage);
}
void FunctionRuntime::loop() {
  if (!core_) return;
  const uint32_t count = core_->mqtt().connectionCount();
  if (count != seenConnectionCount_) { seenConnectionCount_ = count; connected(); }
  servo_.loop(millis());
  digitalSensor_.loop(millis());
  lightSensor_.loop(millis());
  tofSensor_.loop(millis());
}
void FunctionRuntime::onMessage(const String& topic, const String& payload) {
  if (!instance_) return;
  instance_->servo_.onMessage(topic, payload);
  instance_->digitalSensor_.onMessage(topic, payload);
  instance_->lightSensor_.onMessage(topic, payload);
  instance_->tofSensor_.onMessage(topic, payload);
}
void FunctionRuntime::connected() {
  servo_.onConnected();
  digitalSensor_.onConnected();
  lightSensor_.onConnected();
  tofSensor_.onConnected();
}
}  // namespace stapells
