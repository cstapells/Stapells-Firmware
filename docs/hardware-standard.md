# Stapells Node Hardware Standard v1 (draft)

## Logic

Direct ESP GPIO interfaces use 3.3 V logic. USB/VIN and separately powered loads may use other voltages when electrically isolated or correctly interfaced.

## Mandatory health indicator

Every physical node provides exactly four addressable RGB LEDs:

| Index | Role | Healthy/active colour |
|---:|---|---|
| 0 | SYSTEM | Green |
| 1 | WIFI | Blue |
| 2 | MQTT | Purple |
| 3 | ACTIVITY | Orange |

Universal colours:

- Green: healthy/system OK
- Blue: Wi-Fi connected
- Purple: MQTT connected
- Orange: activity
- Yellow: starting, waiting, warning, configuring, or updating
- Red: fault
- Off: unavailable or not configured

The strip performs a four-position walk during boot. Function cards request an activity pulse through Core and never address these LEDs directly.

The production hardware profile must define the health-strip GPIO and the electrical interface. If the chosen pixels are powered at 5 V, the board design must explicitly provide compliant level handling or choose a part suitable for 3.3 V signalling.

## Common conventions

- Serial console: 115200 baud
- Network: Wi-Fi
- Management: MQTT
- Firmware delivery: OTA, including return to the matching Base image
- Peripheral pinning: actual GPIO numbers and capabilities selected by hardware profile/function-card configuration
- Railroad knowledge in Base: none
