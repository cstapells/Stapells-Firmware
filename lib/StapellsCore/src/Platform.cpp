#include "stapells/Platform.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

namespace stapells {
namespace {

String friendlyBoardId(const String& mac) {
  String compact = mac;
  compact.replace(":", "");
  compact.toUpperCase();
  return compact.length() >= 4 ? compact.substring(compact.length() - 4) : compact;
}

}  // namespace

PlatformInfo detectPlatform() {
  PlatformInfo info;

#if defined(ESP8266)
  info.family = PlatformFamily::Esp8266;
  info.name = "ESP8266";
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  info.family = PlatformFamily::Esp32C3;
  info.name = "ESP32-C3";
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  info.family = PlatformFamily::Esp32S3;
  info.name = "ESP32-S3";
#elif defined(ESP32)
  info.family = PlatformFamily::Esp32;
  info.name = "ESP32";
#else
  info.family = PlatformFamily::Unknown;
  info.name = "unknown";
#endif

  info.macAddress = WiFi.macAddress();
  info.boardId = friendlyBoardId(info.macAddress);
  return info;
}

void platformYield() {
  yield();
}

}  // namespace stapells
