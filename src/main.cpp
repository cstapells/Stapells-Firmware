#include <Arduino.h>
#include <StapellsCore.h>

namespace {
stapells::Core core;
}

void setup() {
  Serial.begin(115200);
  core.begin();
}

void loop() {
  core.loop();
}
