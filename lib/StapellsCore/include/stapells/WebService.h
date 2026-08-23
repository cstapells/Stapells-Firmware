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
class WebService {
 public:
  WebService();
  void begin(StatusProvider statusProvider);
  void loop();

 private:
  void sendHome();
  void sendStatus();
  StapellsWebServer server_;
  StatusProvider statusProvider_{nullptr};
};

}  // namespace stapells

