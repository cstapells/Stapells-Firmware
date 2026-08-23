# Base configuration and management

## First boot

Without stored Wi-Fi settings, Base creates an open setup network named
`Stapells-XXXX`, where `XXXX` is the friendly board ID. Open `http://192.168.4.1/`
while connected to that network.

The setup page stores Core configuration in LittleFS and restarts the node. If a
configured station cannot connect within 15 seconds, the setup access point is
also enabled while station retries continue in the background.

## HTTP surface

- `GET /` — Base management and configuration page
- `GET /api/status` — identity, connectivity, state, heap, and uptime JSON
- `POST /api/config` — replace supplied Core settings and restart
- `POST /api/reboot` — restart
- `POST /api/factory-reset` — erase Core configuration and restart
- `GET /update` — ElegantOTA firmware update page

Configuration JSON fields are `wifiSsid`, `wifiPassword`, `mqttHost`,
`mqttPort`, `mqttUsername`, `mqttPassword`, `topicRoot`, `nodeName`,
`healthLedPin`, and `healthBrightness`.

The setup access point is intentionally open in this development milestone so a
new board can always be recovered. Authentication and physical-presence policy
must be selected before a production release.

## MQTT surface

The default node root is `Control/<BOARD_ID>`.

Published topics:

- `status/online` — retained `true`; MQTT last will publishes retained `false`
- `status/summary` — retained JSON status snapshot
- `status/state` — retained lifecycle state
- `identity/platform` — retained processor family
- `identity/firmware` — retained firmware version
- `heartbeat` — uptime seconds every 30 seconds

Subscribed commands:

- `command/status` — publish status immediately
- `command/reboot` — restart the node
- `command/factory-reset` — erase Core configuration only when payload is `CONFIRM`

Function-card topics will be specified separately and must not change these Core
management topics.

## Security boundary

This development Base assumes a trusted layout network. MQTT transport security,
HTTP authentication, signed firmware, setup-AP protection, and command
authorization are explicit production-hardening tasks rather than hidden claims
of this milestone.
