#include "stapells/HealthStrip.h"

namespace stapells {

void HealthStrip::begin(int gpio) {
  gpio_ = gpio;
  // Concrete addressable-LED driver is intentionally isolated here.
}

void HealthStrip::set(HealthLed led, HealthColour colour) {
  (void)led;
  (void)colour;
}

void HealthStrip::pulse(HealthLed led, HealthColour colour) {
  (void)led;
  (void)colour;
}

void HealthStrip::bootWalk() {
  // The production driver will walk all four LEDs once to prove the strip.
}

void HealthStrip::loop() {}

}  // namespace stapells
