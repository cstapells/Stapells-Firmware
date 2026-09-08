# Stapells Firmware

Firmware platform for the physical nodes used on the Stapells Junction N-scale model railroad.

This repository is deliberately separate from `stapells-dispatcher`. It contains microcontroller firmware, shared embedded services, hardware abstractions, and reusable function-card modules.

## Platform model

- **Stapells Core** is linked into every firmware image. It owns identity, Wi-Fi, MQTT, logging, time, OTA, recovery, configuration, and the mandatory health strip.
- **Stapells Base** is a deployable recovery/provisioning image containing Core and no railroad function modules.
- **Function cards** add capabilities such as servos, relays, sensors, signals, lighting, cameras, and distance measurement.
- **Platform abstractions** isolate ESP8266 and ESP32-family differences so modules do not depend on a specific processor.
- The controller/UI owns the desired device definition. Nodes are replaceable commodities.

## Hardware standard

All direct GPIO interfaces use **3.3 V logic**. Every node has four addressable RGB health LEDs in this fixed order:

| LED | Name | Normal colour | Meaning |
|---:|---|---|---|
| 0 | SYSTEM | Green | Firmware/node healthy |
| 1 | WIFI | Blue | Wi-Fi connected |
| 2 | MQTT | Purple | MQTT connected |
| 3 | ACTIVITY | Orange | Module activity |

Yellow means starting/waiting/warning, red means fault, and off means unavailable or not configured.

> The 3.3 V rule is a GPIO logic standard, not a restriction on USB/VIN or separately powered peripherals. The formal hardware design must account for the voltage requirements of the selected RGB LED part.

## Repository layout

```text
src/                    Stapells Base firmware entry point
lib/StapellsCore/       Services shared by every image
modules/                Future function-card modules
boards/                 Hardware profiles and pin capabilities
docs/                   Architecture and platform contracts
test/                   Host and embedded tests
```

## Build targets

PlatformIO environments are provided for the initial processor families:

- Wemos D1 Mini / ESP8266
- Generic ESP32
- ESP32-C3
- ESP32-S3

Core now includes guarded pull-based HTTP OTA for ESP8266 and ESP32-family
targets. GitHub Actions publishes separate factory and OTA application images;
the local Stapells server checks the physical board type and sends a
non-retained, one-board MQTT request. A board accepts only an exact compiled
target match and downloads from its provisioned local server URL. The first
OTA-capable image must still be installed by USB.

After that factory installation, the local browser installer sends Wi-Fi,
MQTT, and the trusted OTA server address directly over the authorized USB
serial connection. Core validates and stores those settings in LittleFS,
acknowledges only the board ID (never a credential), and reboots. Site
credentials are therefore absent from both the public firmware image and MQTT.

## Design rule

> The ESP is disposable. The definition of what it does belongs to Stapells Junction.
