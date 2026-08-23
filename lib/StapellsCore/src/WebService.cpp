#include "stapells/WebService.h"

namespace stapells {

WebService::WebService() : server_(80) {}

void WebService::begin(StatusProvider statusProvider) {
  statusProvider_ = statusProvider;

  server_.on("/", HTTP_GET, [this]() { sendHome(); });
  server_.on("/api/status", HTTP_GET, [this]() { sendStatus(); });
  server_.onNotFound([this]() { server_.send(404, "application/json", "{\"error\":\"not found\"}"); });
  server_.begin();
  Serial.println(F("[web] Read-only diagnostics server started"));
}

void WebService::loop() {
  server_.handleClient();
}

void WebService::sendStatus() {
  server_.send(200, "application/json", statusProvider_ ? statusProvider_() : "{}");
}

void WebService::sendHome() {
  static const char page[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Stapells Base</title><style>
body{font:16px system-ui;max-width:760px;margin:40px auto;padding:0 18px;color:#17202a}
pre{font:14px ui-monospace,monospace;background:#eef2f5;padding:14px;overflow:auto}</style></head><body>
<h1>Stapells Base</h1><pre id="status">Loading...</pre>
<p>This page is intentionally read-only. Configuration and firmware deployment belong to Stapells Junction.</p>
<script>
const req=(url,opt)=>fetch(url,opt).then(r=>r.json());
const refresh=()=>req('/api/status').then(x=>status.textContent=JSON.stringify(x,null,2));refresh();setInterval(refresh,5000);
</script></body></html>)HTML";
  server_.send_P(200, "text/html", page);
}

}  // namespace stapells

