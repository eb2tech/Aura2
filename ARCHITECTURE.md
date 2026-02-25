# Aura2 architecture

Aura2 is a firmware project for a CYD-based landscape weather widget. It uses:

- PlatformIO for builds and dependency management.
- LVGL for UI rendering.
- EEZ Studio for visual UI layout.
- WiFi for external communication, such as weather retrieval.
- MQTT for integration with Home Assistant.
- NATS' MQTT mode for logging, in addition to Serial.

## High-level structure

- `src/` — application logic, UI behavior, networking, and hardware abstraction.
- `lib/` — third-party or shared libraries.
- `include/` — headers for project modules.
- `data/` — static assets if used (fonts, images, etc.).
- `aura2.eez-project` — EEZ Studio project defining UI layout.
- `platformio.ini` — PlatformIO configuration.

## Key flows

- **Startup:** initialize hardware, LVGL, load UI, connect WiFi, connect MQTT.
- **WiFi provisioning:** SoftAP-based flow to configure WiFi credentials.
- **Home Assistant integration:** publish sensor data and subscribe to relevant topics via MQTT.

## Decision history

See `/architecture/adr` for detailed decisions:

- `0001-use-platformio-for-firmware-builds.md`
- `0002-use-eez-studio-for-ui-layout.md`
- `0003-integrate-with-home-assistant-via-mqtt.md`