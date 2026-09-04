# Function-card modules

Function cards are reusable capabilities composed into a node image by the Stapells Junction controller.

Planned examples include PCA9685 servos, PCF8574/PCF8575 I/O, frog relays, IR and voltage-drop sensors, VL53L0X time-of-flight sensors, INA219 current sensing, signals, lighting, worklights, cameras, and accessory outputs.

Rules:

1. A module depends on Stapells Core interfaces, never directly on ESP8266- or ESP32-specific APIs.
2. A module declares hardware requirements and supported targets; it does not hard-code board pin labels.
3. Module presence is a build-time capability decision. Operational values such as turnout endpoints, sensor IDs, thresholds, labels, and MQTT mappings are runtime configuration.
4. Modules signal activity through Core and do not control the mandatory health strip.
5. Modules contain no irreplaceable desired-state data.

## Implemented foundation

`StapellsFunctions` now provides the common runtime and module interface. The
first production module is `TURNOUT_SERVO`, using a PCA9685 and one or two
configured PCF8574 frog-output boards.

The servo module deliberately preserves the installed-layout MQTT contract:

- `track/turnouts/<id>` is `CLOSED` or `THROWN`.
- `/min` is the closed calibration and `/max` is the thrown calibration; their
  numeric values are never reordered.
- `/board`, `/channel`, and `/frog` assign hardware explicitly. The legacy
  `Control/<board-id>_Servos` comma-separated list remains a fallback.
- `Control/<board-id>_Type=SERVO` and a `TURNOUT_SERVO` entry in
  `Control/<board-id>_Functions` both enable the module.

PCF8574 count and addresses come from the board's retained `_Modules` array.
One PCF8574 at `0x20` remains the legacy default. Each configured module can use
any valid PCF8574 address from `0x20` through `0x27`.

On startup a servo output remains untouched until board ownership, channel,
frog assignment, both endpoints, and retained state are known. A turnout is
locked if the PCF8574 serving its frog output is absent. Because physical position cannot
be read after a reboot, the first complete retained state is applied directly;
later state changes retain the legacy count-by-count travel at 42 Hz. Frog
polarity changes only after the servo reaches its endpoint.
