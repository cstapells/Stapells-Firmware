#pragma once

#include <Arduino.h>

namespace stapells {

enum class PlatformFamily : uint8_t {
  Esp8266,
  Esp32,
  Esp32C3,
  Esp32S3,
  Unknown
};

struct PlatformInfo {
  PlatformFamily family{PlatformFamily::Unknown};
  String name{"unknown"};
  String macAddress{};
  String boardId{};
};

PlatformInfo detectPlatform();
void platformYield();

}  // namespace stapells
