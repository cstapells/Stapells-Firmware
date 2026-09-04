#include <StapellsFunctions.h>
namespace stapells {
FunctionRuntime* FunctionRuntime::instance_ = nullptr;
void FunctionRuntime::begin(Core& core) {
  instance_ = this; core_ = &core;
  servo_.begin(core.mqtt(), core.platform().boardId);
  core.setFunctionMessageHandler(onMessage);
}
void FunctionRuntime::loop() {
  if (!core_) return;
  const uint32_t count = core_->mqtt().connectionCount();
  if (count != seenConnectionCount_) { seenConnectionCount_ = count; connected(); }
  servo_.loop(millis());
}
void FunctionRuntime::onMessage(const String& topic, const String& payload) {
  if (instance_) instance_->servo_.onMessage(topic, payload);
}
void FunctionRuntime::connected() { servo_.onConnected(); }
}  // namespace stapells
