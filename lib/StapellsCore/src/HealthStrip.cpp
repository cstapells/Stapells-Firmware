#include "stapells/HealthStrip.h"

namespace stapells {

HealthStrip::~HealthStrip() { delete strip_; }

void HealthStrip::begin(int gpio, uint8_t brightness) {
  delete strip_;
  strip_ = nullptr;
  if (gpio < 0) return;

  strip_ = new Adafruit_NeoPixel(kLedCount, static_cast<uint16_t>(gpio), NEO_GRB + NEO_KHZ800);
  strip_->begin();
  strip_->setBrightness(brightness);
  strip_->clear();
  strip_->show();
}

uint32_t HealthStrip::colour(HealthColour value) const {
  if (!strip_) return 0;
  switch (value) {
    case HealthColour::Green: return strip_->Color(0, 255, 0);
    case HealthColour::Blue: return strip_->Color(0, 0, 255);
    case HealthColour::Purple: return strip_->Color(128, 0, 255);
    case HealthColour::Orange: return strip_->Color(255, 72, 0);
    case HealthColour::Yellow: return strip_->Color(255, 180, 0);
    case HealthColour::Red: return strip_->Color(255, 0, 0);
    case HealthColour::Off: default: return 0;
  }
}

void HealthStrip::set(HealthLed led, HealthColour value) {
  values_[static_cast<uint8_t>(led)] = value;
  render();
}

void HealthStrip::pulse(HealthLed led, HealthColour value, uint16_t durationMs) {
  if (led != HealthLed::Activity) return;
  values_[static_cast<uint8_t>(led)] = value;
  activityPulse_ = true;
  activityUntilMs_ = millis() + durationMs;
  render();
}

void HealthStrip::bootWalk() {
  if (!strip_) return;
  for (uint8_t i = 0; i < kLedCount; ++i) {
    strip_->clear();
    strip_->setPixelColor(i, colour(HealthColour::Yellow));
    strip_->show();
    delay(90);
  }
  strip_->clear();
  strip_->show();
}

void HealthStrip::loop() {
  if (activityPulse_ && static_cast<int32_t>(millis() - activityUntilMs_) >= 0) {
    activityPulse_ = false;
    values_[static_cast<uint8_t>(HealthLed::Activity)] = HealthColour::Off;
    render();
  }
}

void HealthStrip::render() {
  if (!strip_) return;
  for (uint8_t i = 0; i < kLedCount; ++i) strip_->setPixelColor(i, colour(values_[i]));
  strip_->show();
}

}  // namespace stapells
