# Copilot Instructions — Aura2 Firmware

These instructions define how Copilot should understand and contribute to the Aura2 project.  
Use this file as the authoritative guide for architecture, conventions, and boundaries.

---

## Project Overview

Aura2 is a landscape‑layout weather widget firmware for the ESP32‑2432S028R (CYD) board.  
It is built with **PlatformIO**, written in **C++**, and uses:

- **LVGL** for UI rendering  
- **EEZ Studio** as the primary UI layout source  
- **WiFi SoftAP provisioning**  
- **MQTT** for Home Assistant integration  

The goal is a maintainable, reproducible firmware project with clear separation between UI, networking, and hardware abstraction.

---

## Architectural Principles

- **Layout in EEZ Studio, logic in C++**  
  EEZ Studio defines widget hierarchy and geometry. C++ implements behavior, events, and state updates.

- **Separation of concerns**  
  - UI layout (EEZ Studio → generated code)  
  - UI logic (event handlers, state updates)  
  - Networking (WiFi, MQTT)  
  - Hardware abstraction (display, touch, storage)

- **Small, composable modules**  
  Prefer focused files and functions over large monolithic ones.

- **Consistency over cleverness**  
  Follow existing patterns in `src/` and `include/`.

---

## Code Conventions

- **Language:** C++ (Arduino‑style where appropriate)
- **Naming:**
  - Screens/components: `weather_screen`, `status_bar`
  - LVGL objects: consistent prefixes per screen or feature
  - Functions: `create_weather_screen()`, `update_time_label()`
- **Error handling:**  
  Always check return values from LVGL, WiFi, MQTT, and hardware calls.
- **Logging:**  
  Prefer `Log.infoln()` over `Serial.println()` for consistency.
- **Comments:**  
  Comment complex LVGL logic or non‑obvious hardware interactions.

---

## UI & LVGL Guidelines

- Treat **EEZ Studio** as the source of truth for layout.  
  Modify the `.eez-project` file rather than editing generated code.

- Generated files should not be manually edited unless explicitly designed for extension.

- Keep UI logic separate from layout:
  - Layout = EEZ Studio  
  - Behavior = C++ in `src/`  

- LVGL configuration lives in `include/lv_conf.h`.  
  Display driver configuration lives in `include/Setup_ESP32_2432S028R_ILI9341.h`.

---

## Networking, WiFi, and MQTT

- WiFi provisioning uses **SoftAP**. Keep provisioning logic cohesive and well‑documented.
- MQTT topics must follow Home Assistant conventions used in this project.
- When adding MQTT features:
  - Reuse existing connection/reconnection patterns.
  - Centralize topic strings to avoid scattering literals.

---

## Testing & Experimentation

- Use `test/` for automated tests or small harnesses.
- Manual testing is done on hardware using serial output and the device display.
- Keep experimental features isolated and clearly marked.

---

## Architectural References

Copilot should treat these files as authoritative:

- `ARCHITECTURE.md` — system overview and subsystem boundaries  
- `/architecture/adr` — architectural decision history  
- `aura2.eez-project` — UI layout source  
- `platformio.ini` — build configuration  

---

## ADR Workflow

Create or update an ADR when a change:

- introduces a new tool or framework  
- modifies UI architecture or layout strategy  
- alters WiFi provisioning or MQTT integration  
- affects OTA strategy, partitioning, or storage  
- changes hardware abstraction boundaries  

Detailed decision records are stored in the [architecture/adr/](architecture/adr/) directory. Refer to that directory for the current architectural state.

---

## When in Doubt

- Maintain clear separation between layout, UI logic, networking, and hardware.
- Prefer explicit, readable code over clever or compact code.
- Follow existing patterns before introducing new ones.
- If a change feels architectural, write an ADR.

---
