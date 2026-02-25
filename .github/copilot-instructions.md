# Aura ESP32 Weather Widget

Always reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.

## Project overview

- Aura2 is a landscape-layout weather widget firmware for the CYD board.
- It is written primarily in C and built with PlatformIO in Visual Studio Code.
- The UI is rendered with LVGL, with layout defined as much as possible in EEZ Studio.
- The device integrates with Home Assistant via MQTT and supports WiFi provisioning using SoftAP.

## Goals

- Keep the firmware easy to iterate on while experimenting with:
  - LVGL-based UI layouts generated from EEZ Studio.
  - MQTT-based integration with Home Assistant.
  - WiFi provisioning flows using SoftAP.
- Prefer clarity and maintainability over micro-optimizations unless explicitly noted.

## Code conventions

- **Language:** C++ is the primary language; follow existing style in `src/` and `lib/`.
- **Structure:** Prefer small, focused modules over large monolithic files.
- **Naming:**
  - Use descriptive names for UI components and screens (e.g., `weather_screen`, `status_bar`).
  - For LVGL objects, keep a consistent prefix per screen or feature.
  - Use descriptive function names that indicate their purpose (e.g., `create_weather_screen()`, `update_time_label()`).
- **Error handling:** Check return values from hardware, network, and LVGL calls; log or surface failures where possible.
- Follow existing Arduino/C++ conventions
- Comment complex LVGL UI creation code
- **Serial Debug Messages**: Use calm, professional tone for Serial.println() statements. Avoid excessive exclamation points. Prefer "WiFi connected" over "WiFi connected!" and "Setup complete" over "Setup complete!"
- Prefer Log.infoln() for logging instead of Serial.println() for consistency

## UI and LVGL

- Treat EEZ Studio as the source of truth for layout where possible.
- When editing generated code:
  - Avoid manual changes to files clearly marked as generated.
  - Prefer extension points or wrapper functions in `src/` for custom behavior.
- Keep UI logic (event handlers, state updates) separate from layout definitions.
- Layout is done in EEZ Studio and is considered primary. If generated needs modification, consider if it can be done in EEZ Studio first and modify the source .eez-project file instead of the generated code.
- Edit `include/Setup_ESP32_2432S028R_ILI9341.h` for display driver settings
- Current config: ILI9341_2_DRIVER, 240x320 resolution, specific pin mappings
- Edit `include/lv_conf.h` for LVGL library settings (if needed)
- Current config: 16-bit color depth, ESP32 optimizations

## Networking, WiFi, and MQTT

- WiFi provisioning is done via SoftAP; keep related logic cohesive and well-documented.
- MQTT topics and payloads should remain consistent with Home Assistant conventions used in this project.
- When adding new MQTT features:
  - Reuse existing connection and reconnection patterns.
  - Centralize topic definitions and avoid scattering string literals.

## Testing and experimentation

- Use the `test/` folder for any automated tests or small harnesses.
- For manual testing, rely on Serial Monitor output and the device's display for feedback.

## Files to treat as architectural references

- `README.md` — high-level description and build instructions.
- `ARCHITECTURE.md` — over-arching architectural details and system overview.
- `aura2.eez-project` — UI layout source (do not modify blindly; respect EEZ Studio workflows).
- `platformio.ini` — build configuration and environment definitions.

## Prerequisites
- ESP32-2432S028R ILI9341 device with 2.8" screen (CYD/Cheap Yellow Display hardware)
- PlatformIO Core or VS Code with PlatformIO extension installed
- All required libraries will be auto-installed via platformio.ini

## Architectural decisions and history

- Architectural decision records (ADRs) live under `/architecture/adr`.
- When a change:
  - Introduces a new tool or framework,
  - Changes how UI is structured,
  - Alters integration with Home Assistant or WiFi provisioning,
  create or update an ADR instead of embedding long explanations in comments.

Key ADRs (see `/architecture/adr`):

- `0001-use-platformio-for-firmware-builds.md`
- `0002-use-eez-studio-for-ui-layout.md`
- `0003-integrate-with-home-assistant-via-mqtt.md`

When in doubt, prefer:
- Clear separation between UI layout, UI logic, networking, and hardware abstraction.
- Small, composable functions over deeply nested logic.

## CRITICAL LIMITATIONS

**HARDWARE DEPENDENCY**: This project CANNOT be built, tested, or run without the specific ESP32-2432S028R ILI9341 hardware. Do not attempt to build without the hardware.

**PLATFORMIO BUILDS**: This is a PlatformIO project. All compilation must be done through PlatformIO CLI or VS Code with PlatformIO extension.

**NO AUTOMATED TESTING**: There are no unit tests, integration tests, or CI/CD pipelines. All validation must be done manually on the hardware.

**CRITICAL CONFIGURATION STEP**: Configuration files are included in the project:
1. TFT_eSPI configuration: `include/Setup_ESP32_2432S028R_ILI9341.h`
2. LVGL configuration: `include/lv_conf.h` (if needed)
3. Build flags in `platformio.ini` handle automatic inclusion

**IMPORTANT: PlatformIO CLI is NOT available in PATH**
- PlatformIO CLI commands (`pio run`, `platformio run`) will NOT work in terminal
- All builds, uploads, and monitoring MUST be done through VS Code PlatformIO extension UI
- PlatformIO CLI can work if you provide the full path to the executable installed for the user, but this is not recommended for ease of use

**Expected Build Time**: 2-3 minutes for compilation, 1-2 minutes for upload. NEVER CANCEL builds or uploads.

### Key File Locations
```
Repository Structure:
├── data/
│   ├── index.html               # Web-based settings interface hosted on device
│   └── images/                  # Weather icons and images in binary format (.bin)
├── src/
│   ├── main.cpp                 # Main Arduino application
│   ├── forecast_weather.cpp     # Weather logic and image selection (SPIFFS based)
│   ├── extract_unicode_chars.py # Unicode analysis utility
├── include/
│   ├── Setup_ESP32_2432S028R_ILI9341.h  # TFT_eSPI configuration
│   └── lv_conf.h                # LVGL configuration (FS enabled, limited fonts)
├── platformio.ini               # PlatformIO project configuration (Custom OTA partitions)
└── partitions.csv               # Partition table for 4MB ESP32 with dual OTA slots
```

### Modifying Display Configuration

### Font Management
- Custom fonts in `src/lv_font_*.c` files
- Use LVGL Font Converter tool to generate new fonts
- Include Unicode characters: `°¿ÉÊÍÓÜßáäçèéíñóöúûü‐→` (from extract_unicode_chars.py)

## Debugging

### Serial Monitor Output
- Set baud rate to 115200 (configured in platformio.ini)
- Look for WiFi connection status
- Monitor weather API responses
- Check for memory allocation errors

**PlatformIO Serial Monitor:**
```bash
pio device monitor --baud 115200
```

### Common Issues
- **Compilation fails**: Check library versions in platformio.ini and configuration files
- **Display blank**: Check TFT_eSPI configuration in include/Setup_ESP32_2432S028R_ILI9341.h
- **Touch not working**: Verify XPT2046 pin configuration
- **WiFi issues**: Check WifiManager configuration and credentials
- **Weather not updating**: Verify internet connection and API responses
- **collect2.exe error**: Memory/linking issue - check partition scheme and reduce binary size

### Hardware Connections
The TFT_eSPI configuration defines these pin connections:
- TFT_MISO: 12, TFT_MOSI: 13, TFT_SCLK: 14
- TFT_CS: 15, TFT_DC: 2, TFT_RST: -1 (connected to ESP32 reset)
- TOUCH_CS: 33, LCD_BACKLIGHT_PIN: 21

## Project Architecture

### Weather Data Source
- Uses Open-Meteo API for weather data
- Geocoding API for location search
- No API key required
- Updates every 10 minutes (600000ms)

