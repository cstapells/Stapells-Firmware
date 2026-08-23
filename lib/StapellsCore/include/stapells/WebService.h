#pragma once

#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WebServer.h>
using StapellsWebServer = ESP8266WebServer;
#else
#include <WebServer.h>
using StapellsWebServer = WebServer;
#endif

namespace stapells {

using StatusProvider = String (*)();
using ConfigWriter = bool (*)(const String& body, String& error);
using VoidAction = void (*)();

class WebService {
 public:
  WebService();
  void begin(StatusProvider statusProvider, ConfigWriter configWriter,
             VoidAction rebootAction, VoidAction factoryResetAction);
  void loop();

 private:
  void sendHome();
  void sendStatus();
  void saveConfig();

  StapellsWebServer server_;
  StatusProvider statusProvider_{nullptr};
  ConfigWriter configWriter_{nullptr};
  VoidAction rebootAction_{nullptr};
  VoidAction factoryResetAction_{nullptr};
};

}  // namespace stapells
