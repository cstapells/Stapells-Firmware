# Base site configuration and management

## Current development configuration

Base connects only as a station to the Stapells Junction layout Wi-Fi. It never
creates an access point and has no captive portal.

Development site values pass through `stapells/SiteDefaults.h`. The current
compile-time fields are:

- `STAPELLS_WIFI_SSID`
- `STAPELLS_WIFI_PASSWORD`
- `STAPELLS_MQTT_HOST` (currently defaults to `192.168.2.60`)
- `STAPELLS_HEALTH_LED_PIN`

They can be supplied as PlatformIO build definitions or temporarily placed in
`SiteDefaults.h`. Function cards and network code must never read these macros
directly; they consume `CoreConfig`, which keeps the future credential source a
drop-in replacement.

The repository intentionally contains no real Wi-Fi password. The existing
layout values should be inserted locally for a hardware build and must not be
printed in build logs.

## Future credential source

Stapells Junction will later designate SSID and password through a controlled
provisioning/deployment workflow. That implementation will populate the same
`CoreConfig` fields, leaving NetworkService and function cards unchanged.

## HTTP surface

- `GET /` — read-only Base diagnostics page
- `GET /api/status` — identity, connectivity, state, heap, and uptime JSON

There are no web endpoints for configuration, restart, reset, or firmware upload.

## Firmware deployment

ElegantOTA is not part of Stapells Base. Firmware deployment and return-to-Base
will be implemented as a controller-owned service with a narrow Core transport
interface. USB remains the initial flashing and recovery mechanism until that
service is delivered.

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
- `command/factory-reset` — erase stored Core overrides only when payload is `CONFIRM`

Function-card topics are specified separately and cannot change these Core
management topics.

## Security boundary

This development Base assumes a trusted layout network. MQTT transport security,
credential provisioning, signed firmware, and command authorization remain
explicit production-hardening work.

