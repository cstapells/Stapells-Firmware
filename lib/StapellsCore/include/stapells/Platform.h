#pragma once

#include <Arduino.h>

namespace stapells {

enum class PlatformFamily : uint8_t { Esp8266, Esp32, Esp32C3, Esp32S3, Unknown };

struct PlatformInfo {
  PlatformFamily family{PlatformFamily::Unknown};
  String name{"unknown"};
  String macAddress{};
  String boardId{};
  String resetReason{};
  uint32_t flashSize{0};
};

PlatformInfo detectPlatform();
uint32_t freeHeap();
void platformYield();
void restartPlatform();

}  // namespace stapells
