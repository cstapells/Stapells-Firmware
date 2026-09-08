# Stapells Junction MQTT strategy

**Status: canonical design specification**

This file is the authority for MQTT topic design across the Stapells Junction
control site and firmware. When an older sketch, implementation, or note differs
from this file, preserve installed-layout compatibility and update the newer
work to follow this strategy.

## Design principles

1. **JMRI-facing operational topics stay stable.** Existing state topics and
   payload spelling are not replaced with JSON or moved.
2. **Configuration belongs to the physical board.** Board ownership, attached
   modules, channel mappings, thresholds, polarity, and timing settings live
   under `Control/<board>...`, not beneath every sensor state topic.
3. **MQTT configuration is retained.** A replacement board can recover its job
   after connecting. Live logs are not retained.
4. **The controller owns the definition.** A board consumes configuration and
   reports status; it does not invent a second authoritative configuration.
5. **Credentials never appear in MQTT.** Broker credentials stay in local
   provisioning storage and must not be logged or published.
6. **Add around established contracts.** New metadata must not break JMRI or
   alter the installed turnout and sensor topics.

## Board identity

`<board>` is the final four uppercase hexadecimal characters of the Wi-Fi
station MAC address. Colons are removed before taking the final four characters.
For example, a MAC ending in `:5C:3F` produces board ID `5C3F`.

The four-character value is a friendly layout identifier, not a globally unique
identity. The full MAC remains available for diagnosing the unlikely case of a
collision.

## Common board topics

These retained topics are common to every board:

| Topic | Meaning | Example |
|---|---|---|
| `Control/<board>` | Heartbeat timestamp | `2026-09-05 14:30:00` |
| `Control/<board>_Name` | Friendly board name | `East Yard Sensors` |
| `Control/<board>_Type` | Primary role | `SENSOR` |
| `Control/<board>_BoardType` | Physical controller and pinout | `esp32-c3` |
| `Control/<board>_Functions` | Compact JSON array of capabilities | `["DIGITAL_SENSOR"]` |
| `Control/<board>_Modules` | Compact JSON array of attached hardware | see below |
| `Control/<board>_IP` | Current IP address | `192.168.2.81` |
| `Control/<board>_Version` | Firmware version | `0.3.0` |
| `Control/<board>_Status` | Compatibility status | `ONLINE` or `OFFLINE` |
| `Control/<board>_OtaCapable` | This firmware can perform guarded pull-based OTA | `TRUE` |

`Control/<board>/Logs` carries live diagnostic messages and is **not retained**.
Detailed function health may be retained beneath:

```text
Control/<board>/status/functions/<function>
```

Standard function-health values are `READY`, `NOT_CONFIGURED`,
`FAULT_HARDWARE`, and `FAULT_CONFIG`. MQTT last will reports `OFFLINE` through
`Control/<board>_Status`, and a successful connection publishes `ONLINE`.
Do not also publish a duplicate `/status/online` topic.

OTA commands are live and never retained:

```text
Control/<board>/command/update
```

The compact command must include the exact compiled board type, an update
request ID, the desired release identifier, and `"confirm":"APPLY"`. The
firmware downloads only from its locally provisioned Stapells server URL; it
never accepts an arbitrary download URL from MQTT. Progress is retained at
`Control/<board>/ota/status` so the controller can show the last result. Factory
images and OTA application images are separate release files. The first
OTA-capable installation is always made by USB.

`BoardType` is the public configuration name because it identifies the complete
physical board and its pinout. Build code may continue to call the equivalent
value a `target`. The former `Control/<board>_Target` spelling is accepted only
as a migration input and is not published by new work.

## Connection behavior

- Broker host, port, optional username, and password come from local
  provisioning rather than function source code.
- Client ID is `stapells-<board>`.
- Keep-alive is 30 seconds.
- Reconnection is non-blocking and normally retried every five seconds.
- Heartbeat is normally published every 60 seconds.
- On connection, a board subscribes to its own retained control configuration
  and only the layout topics required by its functions.
- Loss of MQTT must not cause unsafe movement or erase the last valid
  configuration.

## Board-level arrays and payload size

`_Functions` and `_Modules` describe the board, not every layout object it
controls. Publish JSON compactly, without formatting whitespace:

```json
["DIGITAL_SENSOR","BH1750_SENSOR"]
```

```json
[{"type":"PCF8575","address":"0x20"},{"type":"PCF8575","address":"0x21"}]
```

The shared PubSubClient buffer is currently 1,024 bytes for the entire MQTT
packet. Keep each configuration payload at or below **750 bytes** so the topic
and protocol overhead have comfortable headroom. The control site must validate
this before publishing. Large definitions are divided into meaningful
board-level control topics rather than placed in one oversized document.

## Visual wiring workspace

Each board may have a visual wiring workspace saved in the local control-site
registry. It records diagram positions, connector choices, wires, and labels for
that board. This full drawing is design data and is **not** published to MQTT.

The control site derives the compact `_Modules`, `_Functions`, `_Sensors`, and
function configuration payloads from the approved board configuration. This
keeps firmware messages small while allowing the UI to check recommended I2C
pins, power, ground, boot-sensitive GPIO pins, address conflicts, TCA9548A
routes, and PCF8575 channel-to-sensor assignments.

VCC recommendations are module-aware. The workspace uses 3.3 V as the normal
logic supply for ESP-connected I2C modules, while permitting 5 V for parts whose
documented supply range allows it. A 5 V VCC connection remains visibly marked
and produces a warning when a breakout may pull SDA/SCL up to its supply. Servo
power remains a separate PCA9685 `V+` connection and must never be inferred from
logic `VCC`.

## Sensors

JMRI consumes only the established state topic:

```text
track/sensors/<sensor-id> = ACTIVE
track/sensors/<sensor-id> = INACTIVE
```

Do not replace these payloads with JSON. Topic spelling and lowercase
`track/sensors/` are part of the compatibility contract.

Optional numeric telemetry may use:

```text
track/sensors/<sensor-id>/value
```

JMRI does not need the telemetry topic. It exists for diagnostics and the local
control interface.

Sensor configuration stays at board level:

```text
Control/<board>_Sensors
Control/<board>_SensorConfig
```

`_Sensors` preserves the existing comma-separated, channel-ordered mapping. For
a two-PCF8575 board, the first value maps to channel 0 and the thirty-second to
channel 31:

```text
Control/5C3F_Sensors = 12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43
Control/5C3F_SensorConfig = {"polarity":"ACTIVE_LOW","debounce_ms":50}
```

Use `-` or a blank position for an unused channel so later channel positions do
not shift. Optional `invert_channels` and `debounce_overrides` provide compact
exceptions without creating per-sensor configuration topics:

```text
Control/5C3F_Sensors = 12,13,-,15
Control/5C3F_SensorConfig = {"polarity":"ACTIVE_LOW","debounce_ms":50,"invert_channels":[3],"debounce_overrides":{"1":100}}
```

A single-sensor BH1750 board uses the same ownership pattern:

```text
Control/A17C_Sensors = 25
Control/A17C_SensorConfig = {"sample_ms":250,"report_ms":5000,"calibration_ms":15000,"baseline_refresh_ms":300000,"active_drop":40,"clear_drop":25,"learn_rate":0.05,"minimum_lux":1}
```

A single-sensor VL53L0X board likewise keeps its configuration with the board:

```text
Control/B82D_Sensors = 40
Control/B82D_SensorConfig = {"read_interval_ms":1000,"minimum_mm":10,"maximum_mm":80}
```

When several fixed-address VL53L0X sensors share one controller, their compact
connection routes stay in the board's `_Modules` inventory. The order of the
`TOF` entries maps directly to the order in `_Sensors`; all sensors continue to
share the board-level distance settings:

```text
Control/B82D_Modules = [{"type":"TCA9548A","address":"0x70"},{"type":"TOF","address":"0x29","mux_address":"0x70","mux_channel":0},{"type":"TOF","address":"0x29","mux_address":"0x70","mux_channel":1}]
Control/B82D_Sensors = 40,41
Control/B82D_SensorConfig = {"read_interval_ms":1000,"minimum_mm":10,"maximum_mm":80}
```

`mux_address` accepts `0x70`-`0x77` and `mux_channel` accepts `0`-`7`. A direct
VL53L0X omits both fields. Routes must be unique, and a routed sensor requires a
matching `TCA9548A` module entry. This adds connection information only where it
is needed; it does not create per-sensor configuration topics.

Mixed sensor boards use concise function-specific overrides:

```text
Control/<board>_DigitalSensors
Control/<board>_DigitalSensorConfig
Control/<board>_BH1750Sensors
Control/<board>_BH1750Config
Control/<board>_ToFSensors
Control/<board>_ToFConfig
```

Single-purpose boards use `_Sensors` and `_SensorConfig`. Do not create `/type`,
`/board`, `/channel`, and `/config/...` trees beneath every
`track/sensors/<id>` unless this specification is deliberately revised.

## Turnouts

The installed turnout contract is an explicit compatibility exception and must
remain unchanged:

```text
track/turnouts/<id>
track/turnouts/<id>/min
track/turnouts/<id>/max
track/turnouts/<id>/board
track/turnouts/<id>/channel
track/turnouts/<id>/frog
```

`min` retains its CLOSED meaning and `max` retains its THROWN meaning. Their
numeric values are never reordered. No servo moves until ownership, channel,
frog assignment, both endpoints, and retained state are known. Powered-frog
polarity changes only after movement finishes.

## Change rule

Before introducing or renaming an MQTT topic:

1. Check this strategy and the installed JMRI contract.
2. Prefer an existing board-level `Control/<board>...` topic.
3. Confirm retained versus live behavior.
4. Confirm the compact payload remains within 750 bytes.
5. Provide backward compatibility before changing any established topic.
