#include "stapells/WebService.h"

#include <ElegantOTA.h>

namespace stapells {

WebService::WebService() : server_(80) {}

void WebService::begin(StatusProvider statusProvider, ConfigWriter configWriter,
                       VoidAction rebootAction, VoidAction factoryResetAction) {
  statusProvider_ = statusProvider;
  configWriter_ = configWriter;
  rebootAction_ = rebootAction;
  factoryResetAction_ = factoryResetAction;

  server_.on("/", HTTP_GET, [this]() { sendHome(); });
  server_.on("/api/status", HTTP_GET, [this]() { sendStatus(); });
  server_.on("/api/config", HTTP_POST, [this]() { saveConfig(); });
  server_.on("/api/reboot", HTTP_POST, [this]() {
    server_.send(202, "application/json", "{\"ok\":true,\"rebooting\":true}");
    delay(100);
    if (rebootAction_) rebootAction_();
  });
  server_.on("/api/factory-reset", HTTP_POST, [this]() {
    server_.send(202, "application/json", "{\"ok\":true,\"factoryReset\":true}");
    delay(100);
    if (factoryResetAction_) factoryResetAction_();
  });
  server_.onNotFound([this]() { server_.send(404, "application/json", "{\"error\":\"not found\"}"); });

  ElegantOTA.begin(&server_);
  server_.begin();
  Serial.println(F("[web] Status and OTA server started"));
}

void WebService::loop() {
  server_.handleClient();
  ElegantOTA.loop();
}

void WebService::sendStatus() {
  server_.send(200, "application/json", statusProvider_ ? statusProvider_() : "{}");
}

void WebService::saveConfig() {
  if (!server_.hasArg("plain")) {
    server_.send(400, "application/json", "{\"error\":\"JSON body required\"}");
    return;
  }
  String error;
  if (!configWriter_ || !configWriter_(server_.arg("plain"), error)) {
    server_.send(400, "application/json", String("{\"error\":\"") + error + "\"}");
    return;
  }
  server_.send(202, "application/json", "{\"ok\":true,\"rebooting\":true}");
  delay(150);
  if (rebootAction_) rebootAction_();
}

void WebService::sendHome() {
  static const char page[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Stapells Base</title><style>
body{font:16px system-ui;max-width:760px;margin:40px auto;padding:0 18px;color:#17202a}
pre,input,button{font:inherit}input{box-sizing:border-box;width:100%;padding:9px;margin:4px 0 12px}
button,a{display:inline-block;padding:10px 14px;margin:4px;background:#174a7e;color:white;border:0;border-radius:6px;text-decoration:none}
pre{background:#eef2f5;padding:14px;overflow:auto}.danger{background:#a93226}</style></head><body>
<h1>Stapells Base</h1><pre id="status">Loading...</pre>
<p><a href="/update">Firmware update</a></p>
<h2>Core configuration</h2><form id="config">
<label>Wi-Fi name<input name="wifiSsid" required></label>
<label>Wi-Fi password<input name="wifiPassword" type="password"></label>
<label>MQTT host<input name="mqttHost" required></label>
<label>MQTT port<input name="mqttPort" type="number" value="1883"></label>
<label>MQTT username<input name="mqttUsername"></label>
<label>MQTT password<input name="mqttPassword" type="password"></label>
<label>Health strip GPIO<input name="healthLedPin" type="number" required></label>
<label>LED brightness (0-255)<input name="healthBrightness" type="number" value="24"></label>
<button>Save and restart</button></form>
<p><button id="restart">Restart</button><button class="danger" id="reset">Factory reset</button></p>
<script>
const req=(url,opt)=>fetch(url,opt).then(r=>r.json());
const refresh=()=>req('/api/status').then(x=>status.textContent=JSON.stringify(x,null,2));refresh();setInterval(refresh,5000);
config.onsubmit=e=>{e.preventDefault();const d=Object.fromEntries(new FormData(config));d.mqttPort=+d.mqttPort;d.healthLedPin=+d.healthLedPin;d.healthBrightness=+d.healthBrightness;req('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)}).then(x=>alert(JSON.stringify(x)))};
restart.onclick=()=>req('/api/reboot',{method:'POST'});reset.onclick=()=>confirm('Erase Core configuration?')&&req('/api/factory-reset',{method:'POST'});
</script></body></html>)HTML";
  server_.send_P(200, "text/html", page);
}

}  // namespace stapells
