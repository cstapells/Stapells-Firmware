# First reference function card: sensor board

After Base passes on one ESP8266 and one ESP32-family target, the first composed
firmware will be a new sensor node rather than a port of the legacy servo sketch.

The sensor card will prove the extension boundary with:

- one simple digital input first;
- configurable GPIO, active level, debounce, sensor ID, and MQTT mapping;
- all assignments held as runtime configuration;
- activity reported through `Core::activity()`;
- no direct Wi-Fi, MQTT, OTA, filesystem, or health-strip ownership;
- operation continuing from stored configuration during controller or MQTT loss.

Once that path is stable, I/O expander and additional sensor types can reuse the
same card contract.
