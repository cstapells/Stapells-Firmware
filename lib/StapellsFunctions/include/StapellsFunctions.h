#pragma once
#include <StapellsCore.h>
#include "stapells/functions/ServoFunction.h"
namespace stapells {
// Dispatches shared services to independently implemented function modules.
class FunctionRuntime {
 public:
  void begin(Core& core);
  void loop();
 private:
  static FunctionRuntime* instance_;
  static void onMessage(const String&, const String&);
  void connected();
  Core* core_{nullptr};
  ServoFunction servo_{};
  uint32_t seenConnectionCount_{0};
};
}  // namespace stapells
