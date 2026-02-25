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

All significant architectural decisions are documented as Architecture Decision Records (ADRs) in the [architecture/adr/](architecture/adr/) directory. Always refer to these documents when making changes that affect the system structure or core technologies.
