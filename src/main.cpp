#include <Arduino.h>
#include <StapellsCore.h>
#include <StapellsFunctions.h>

namespace {
stapells::Core core;
stapells::FunctionRuntime functions;
}

void setup() {
  Serial.setRxBufferSize(2048);
  Serial.begin(115200);
  core.begin();
  functions.begin(core);
}

void loop() {
  core.loop();
  functions.loop();
}
