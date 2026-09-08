# Base/Core lifecycle

The first Core state model is:

```text
BOOTING
  -> CONNECTING_WIFI
  -> CONNECTING_MQTT
  -> UNCONFIGURED | OPERATIONAL

Any managed state
  -> UPDATING
  -> board-matched application image verification
  -> reboot

Any recoverable state
  -> RECOVERING
  -> Base image
  -> UNCONFIGURED

Any fatal initialization failure
  -> FAULT
```

Core owns transitions, health indication, reconnect/backoff, watchdog-friendly service loops, and recovery entry. Function cards are initialized only after Core has loaded and validated the device recipe.

An OTA request is accepted only on the board's own non-retained MQTT command
topic and only when its target exactly matches the image compiled for that
board. The device constructs the download URL from its locally provisioned OTA
server, rather than trusting a URL carried in the command.

A production implementation will distinguish degraded operation from a fatal fault so an already-configured node can continue its physical function during a temporary MQTT or controller outage.
