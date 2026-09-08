# Sensor firmware inventory

This inventory records the behaviour recovered from the earlier installed-layout
sketches. Credentials and network-specific values are deliberately omitted.
The canonical topic and configuration rules are defined in
[`mqtt-strategy.md`](mqtt-strategy.md). This file records sensor-specific
behaviour and defaults only.

## PCF8575 digital inputs

The two earlier sketches use one or two PCF8575 expanders at `0x20` and `0x21`,
with 16 inputs per device. The newer sketch maps physical inputs to layout sensor
IDs and publishes retained state changes. The modular implementation keeps that
capability while moving mapping, polarity, debounce, and addresses into retained
configuration.

The channel-ordered mapping is retained in `Control/<board>_Sensors`, with
polarity and debounce stored in `Control/<board>_SensorConfig`. Channels `0`-`15`
use the first configured PCF8575 and channels `16`-`31` use the second. Only the
retained `ACTIVE` or `INACTIVE` state is published at
`track/sensors/<sensor-id>` for JMRI.

PCF8575 modules and their I2C addresses come from
`Control/<board>_Modules`. Up to two devices are accepted at `0x20`-`0x27`.
Unused channel positions may be written as `-`. Duplicate sensor IDs are ignored
so one physical input cannot accidentally compete with another for the same
JMRI topic. Board defaults can be compactly overridden for selected channels
with `invert_channels` and `debounce_overrides`.

## BH1750 light detection

The recovered BH1750 sketch establishes these field-tested defaults, which the
modular implementation preserves:

| Setting | Default |
|---|---:|
| Sampling | 250 ms |
| Lux reporting | 5 s |
| Boot calibration | 15 s |
| Clear-track baseline refresh | 5 min |
| Active threshold | 40% below baseline |
| Clear threshold | 25% below baseline |
| Baseline learning rate | 0.05 |
| Minimum baseline | 1 lux |

The board stores its sensor ID in `Control/<board>_Sensors` and its settings in
`Control/<board>_SensorConfig`. It publishes retained `ACTIVE`/`INACTIVE` state
at `track/sensors/<sensor-id>` plus optional lux at `/value`. A BH1750 module may
use its valid address `0x23` or `0x5C`.

## Time of flight

The recovered `ESP32_VL53L0X_MQTT_ToF_Window_NoOTA_v1_7.ino` sketch is a
VL53L0X speed-gate/window detector for ESP32 and ESP32-C3. It uses the sensor's
fixed default I2C address `0x29` and an inclusive active-distance window:

```text
ACTIVE when minimum_mm <= measured distance <= maximum_mm
INACTIVE when outside the window, out of range, or the sensor is missing
```

Recovered defaults and accepted limits:

| Setting | Default | Accepted range |
|---|---:|---:|
| Read and report interval | 1000 ms | 25-60000 ms |
| Active-window minimum | 10 mm | 1-1200 mm |
| Active-window maximum | 80 mm | 1-1200 mm |
| Missing-sensor retry | 5 s | fixed in the sketch |
| Board heartbeat | 60 s | fixed in the sketch |

Both ends of the window are retained exactly as configured. They are not
reordered. If the minimum is greater than the maximum, the window is invalid
and the detector remains `INACTIVE` until corrected.

The recovered version intentionally has no debounce, hysteresis,
consecutive-reading filter, timing-budget setting, maximum-range mode, or
TCA9548A multiplexer support. The modular implementation now adds optional
TCA9548A routing while preserving the recovered direct-wiring behaviour and
defaults. Every valid reading is still evaluated immediately.
The device scans I2C at startup, reports whether `0x29` was found, and retries
initialization every five seconds while it is missing.

Legacy retained configuration:

- `Control/<board>_Type`: `TOF_SPEED`
- `Control/<board>_SpeedTopic`: defaults to `track/tof/<board>`
- `Control/<board>_ReadIntervalMS`: defaults to `1000`
- `Control/<board>_TofMinMM`: defaults to `10`
- `Control/<board>_TofThresholdMM`: defaults to `80`

Legacy retained output:

- Configured speed topic: `ACTIVE` or `INACTIVE`
- `<speed-topic>/mm`: distance in millimetres, or `-1` when invalid
- `<speed-topic>/raw`: `VALID`, `OUT_OF_RANGE`, or `TOF_MISSING`
- `<speed-topic>/window`: the configured `minimum-maximum` window
- `Control/<board>_TofStatus`: `FOUND` or `MISSING`
- `Control/<board>_I2CScan`: discovered I2C addresses

For the modular schema, the sensor ID belongs in `Control/<board>_Sensors` and
the read interval plus minimum and maximum distances belong in
`Control/<board>_SensorConfig`. The board publishes retained state at
`track/sensors/<id>` and optional distance telemetry at
`track/sensors/<id>/value`.

The newer implementation preserves these field-tested defaults first.
Hysteresis, filtering, configurable sensor addressing, timing budget, and
VL53L1X range modes can be added later as explicit options rather than silently
changing the original detector.

Up to eight VL53L0X sensors may be connected through TCA9548A channels. Each
`TOF` entry in `Control/<board>_Modules` may include `mux_address` and
`mux_channel`; its position maps to the corresponding sensor number in
`Control/<board>_Sensors`. Before initialization and every reading, firmware
disables the other configured multiplexer paths and selects only the requested
channel. This prevents identical `0x29` devices from answering together. Direct
I2C remains the default for a single sensor.

The modular VL53L0X implementation also consumes the recovered legacy
`TOF_SPEED`, `_SpeedTopic`, `_ReadIntervalMS`, `_TofMinMM`, and
`_TofThresholdMM` configuration. When those legacy topics are present, it
continues publishing the configured `track/tof/...` state, `/mm`, `/raw`, and
`/window` topics alongside the board-level sensor contract.

## Implemented sensor modules

The shared runtime now includes all three recovered sensor families:

- `DIGITAL_SENSOR` / `BLOCK_DETECTION`: one or two PCF8575 expanders and up to
  32 channel-ordered sensor IDs.
- `BH1750_SENSOR`: one adaptive light-based block detector.
- `VL53L0X_SENSOR` / `TOF_DETECTION`: up to eight inclusive distance-window
  detectors, including optional TCA9548A routing and the recovered legacy
  speed-gate contract for the first detector.

Each module remains inactive until its hardware appears in the retained
`_Modules` inventory or an explicit legacy ToF type enables it. Missing hardware
reports `FAULT_HARDWARE`; an installed module without an assigned sensor reports
`NOT_CONFIGURED`. No sensor module moves or actuates layout hardware.

## Programmatic board ID

The board ID is derived from the Wi-Fi station MAC address, so the same method
works on the ESP8266 D1 Mini, ESP32, ESP32-C3, and ESP32-S3:

1. Put Wi-Fi into station mode so the station MAC is available.
2. Read the MAC address from `WiFi.macAddress()`.
3. Remove the colons.
4. Convert the result to uppercase.
5. Use the final four hexadecimal characters.

For example, a station MAC ending in `:5C:3F` produces board ID `5C3F` and a
control root of `Control/5C3F`.

The shared firmware performs this centrally in `StapellsCore`:

```cpp
String friendlyBoardId(const String& mac) {
  String compact = mac;
  compact.replace(":", "");
  compact.toUpperCase();
  return compact.length() >= 4
      ? compact.substring(compact.length() - 4)
      : compact;
}
```

The older ToF sketch performs the equivalent operation directly on the final
two MAC bytes:

```cpp
uint8_t mac[6];
WiFi.macAddress(mac);
char id[5];
sprintf(id, "%02X%02X", mac[4], mac[5]);
```

Both methods produce the same four-character uppercase ID. This is a friendly
layout identifier rather than a globally unique hardware identity; the full
MAC remains available for diagnosing the unlikely case of a four-character
collision.
