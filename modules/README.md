# Function-card modules

Function cards are reusable capabilities composed into a node image by the Stapells Junction controller.

Planned examples include PCA9685 servos, PCF8574/PCF8575 I/O, frog relays, IR and voltage-drop sensors, VL53L0X time-of-flight sensors, INA219 current sensing, signals, lighting, worklights, cameras, and accessory outputs.

Rules:

1. A module depends on Stapells Core interfaces, never directly on ESP8266- or ESP32-specific APIs.
2. A module declares hardware requirements and supported targets; it does not hard-code board pin labels.
3. Module presence is a build-time capability decision. Operational values such as turnout endpoints, sensor IDs, thresholds, labels, and MQTT mappings are runtime configuration.
4. Modules signal activity through Core and do not control the mandatory health strip.
5. Modules contain no irreplaceable desired-state data.
