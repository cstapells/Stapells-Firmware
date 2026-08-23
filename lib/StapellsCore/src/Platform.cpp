#include "stapells/Platform.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <esp_system.h>
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
}

PlatformInfo detectPlatform() {
  PlatformInfo info;
  WiFi.mode(WIFI_STA);
  delay(1);
#if defined(ESP8266)
  info.family = PlatformFamily::Esp8266;
  info.name = "ESP8266";
  info.resetReason = ESP.getResetReason();
  info.flashSize = ESP.getFlashChipRealSize();
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  info.family = PlatformFamily::Esp32C3;
  info.name = "ESP32-C3";
  info.resetReason = String(static_cast<int>(esp_reset_reason()));
  info.flashSize = ESP.getFlashChipSize();
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  info.family = PlatformFamily::Esp32S3;
  info.name = "ESP32-S3";
  info.resetReason = String(static_cast<int>(esp_reset_reason()));
  info.flashSize = ESP.getFlashChipSize();
#elif defined(ESP32)
  info.family = PlatformFamily::Esp32;
  info.name = "ESP32";
  info.resetReason = String(static_cast<int>(esp_reset_reason()));
  info.flashSize = ESP.getFlashChipSize();
#endif
  info.macAddress = WiFi.macAddress();
  info.boardId = friendlyBoardId(info.macAddress);
  return info;
}

uint32_t freeHeap() { return ESP.getFreeHeap(); }
void platformYield() { yield(); }
void restartPlatform() { ESP.restart(); }

}  // namespace stapells
