# Base/Core lifecycle

The first Core state model is:

```text
BOOTING
  -> CONNECTING_WIFI
  -> CONNECTING_MQTT
  -> UNCONFIGURED | OPERATIONAL

Any managed state
  -> UPDATING
  -> reboot

Any recoverable state
  -> RECOVERING
  -> Base image
  -> UNCONFIGURED

Any fatal initialization failure
  -> FAULT
```

Core owns transitions, health indication, reconnect/backoff, watchdog-friendly service loops, and recovery entry. Function cards are initialized only after Core has loaded and validated the device recipe.

A production implementation will distinguish degraded operation from a fatal fault so an already-configured node can continue its physical function during a temporary MQTT or controller outage.
