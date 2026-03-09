# 0004 – OTA Update Strategy

- **Status:** Proposed
- **Date:** 2026-02-25

## Context

The Aura2 firmware needs a reliable Over-the-Air (OTA) update mechanism to support remote updates. The primary constraints are the 4MB flash size of the ESP32-2432S028R (CYD) board and the large size of UI assets (fonts and images). Compiling these assets directly into the firmware binary (as C arrays) would cause the firmware to exceed the available space in a dual-partition OTA setup.

## Decision

Adopt a comprehensive OTA and asset management strategy to ensure updates are possible within the 4MB flash limit.

### 1. New Partition Scheme

Update the `partitions.csv` to support a robust dual-app OTA setup with room for a filesystem:

- **nvs**: 0x5000 (Standard storage for settings)
- **otadata**: 0x2000 (Required for OTA state)
- **app0**: 1512K (OTA slot 0)
- **app1**: 1512K (OTA slot 1)
- **spiffs/littlefs**: 960K (Storage for UI assets)

### 2. UI Asset Management

To keep the application binary size under the 1512K limit:

- **Font Reduction:** Remove all but two essential fonts. The primary UI will use:
  - `Montserrat 14` (Main interface text)
  - `Montserrat 28` (Large values like temperature/time)
- **Binary Image Assets:** Move all icons and images from `src/ui/images.c` to external binary files in the `data/` directory. These assets will be loaded at runtime from the filesystem (`LittleFS`) rather than being compiled into the firmware.
- **FS Management:** Use `LittleFS` for better performance and wear-leveling compared to SPIFFS.

### 3. Update Mechanism

- Use the `ArduinoOTA` library for local network updates.
- Future-proof for remote HTTPS updates by reserving sufficient space in the `app` partitions.

## Consequences

- **Positive:**
  - Enables updates without physical access.
  - Keeps firmware binary size small and manageable.
  - UI assets can be updated independently of the logic by flashing only the filesystem.
- **Negative:**
  - Runtime overhead for loading assets from flash filesystem.
  - Increased complexity in the UI code for handling file-based images.
  - Memory management is more critical as internal assets are no longer in ROM but loaded into RAM or cached from FS.
