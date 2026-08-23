# Firmware architecture

## Layers

```text
Stapells Base image
└── Stapells Core
    ├── lifecycle/state machine
    ├── identity and version
    ├── Wi-Fi and provisioning
    ├── MQTT and discovery
    ├── logging and diagnostics
    ├── time
    ├── OTA and return-to-Base
    ├── configuration storage
    ├── recovery
    ├── four-LED health strip
    └── platform abstraction
        ├── ESP8266
        └── ESP32 family
            ├── ESP32
            ├── ESP32-C3
            └── ESP32-S3

Generated node image
├── Stapells Core
└── selected function-card modules
```

Base contains no railroad knowledge. Core is present in Base and every generated node image.

## Composition boundary

Adding or removing hardware capabilities creates and deploys a whole firmware image. Microcontrollers do not dynamically load compiled C++ cards.

Runtime configuration does not require a rebuild. Servo endpoints, turnout and sensor IDs, thresholds, labels, channel assignments, polarity, and MQTT mappings belong in persistent configuration delivered by the controller.

## Ownership

The controller and its database own desired state. MQTT remains the real-time management and control bus, and nodes must continue their configured function when the management UI/database is temporarily unavailable.

## Identity

Core retains the full MAC address internally and derives the friendly board ID from the final four hexadecimal MAC digits. Replacement workflows must not treat that friendly ID as the only durable identity.

## Dependency direction

Function cards depend inward on Core contracts. Core depends only on portable interfaces and its platform adapters. No function card may pull railroad assumptions into Core.
