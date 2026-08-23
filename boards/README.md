# Hardware profiles

Hardware profiles translate a physical board into processor family, flash layout, usable GPIOs, aliases, and pin capabilities.

Initial target families:

- Wemos D1 Mini / ESP8266
- Generic ESP32
- ESP32-C3 boards
- ESP32-S3 boards
- LOLIN variants where their pin/flash layouts differ

Profiles will expose actual GPIO numbers internally. UI aliases such as `D1 / GPIO5` may be shown to help with legacy hardware, but firmware modules must not use Arduino board aliases such as `D1` or `D5`.

The health-strip GPIO is a Core setting selected from a compatible output pin. All other assignments belong to function cards.
