# Aura ESP32 Weather Widget

Always reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.

Aura is an ESP32-based weather widget that runs on ESP32-2432S028R ILI9341 devices with a 2.8" screen (sometimes called "CYD" or Cheap Yellow Display). This is a hardware-specific PlatformIO project that requires physical ESP32 hardware to build and test.

## CRITICAL LIMITATIONS

**HARDWARE DEPENDENCY**: This project CANNOT be built, tested, or run without the specific ESP32-2432S028R ILI9341 hardware. Do not attempt to build without the hardware.

**PLATFORMIO BUILDS**: This is a PlatformIO project. All compilation must be done through PlatformIO CLI or VS Code with PlatformIO extension.

**NO AUTOMATED TESTING**: There are no unit tests, integration tests, or CI/CD pipelines. All validation must be done manually on the hardware.

## Working Effectively

### Prerequisites
- ESP32-2432S028R ILI9341 device with 2.8" screen (CYD/Cheap Yellow Display hardware)
- PlatformIO Core or VS Code with PlatformIO extension installed
- All required libraries will be auto-installed via platformio.ini

### PlatformIO Configuration
The project is pre-configured in `platformio.ini` with these settings:
- Board: "esp32dev"
- Framework: "arduino"
- Partition Scheme: "huge_app.csv"
- Monitor Speed: 115200 baud
- Flash Size: 4MB

### Required Libraries (Auto-installed)
These libraries are automatically installed via `platformio.ini`:
- ArduinoJson 7.4.2
- lvgl 9.3.0
- TFT_eSPI 2.5.43
- WiFiManager 2.0.17
- XPT2046_Touchscreen 1.4 (from GitHub)

**CRITICAL CONFIGURATION STEP**: Configuration files are included in the project:
1. TFT_eSPI configuration: `include/Setup_ESP32_2432S028R_ILI9341.h`
2. LVGL configuration: `include/lv_conf.h` (if needed)
3. Build flags in `platformio.ini` handle automatic inclusion

### Project Setup
1. Clone or copy the project to your desired location
2. Open project folder in VS Code with PlatformIO extension, or use PlatformIO CLI
3. Connect ESP32 hardware via USB
4. PlatformIO will auto-install all dependencies on first build

### Build Process
**Using VS Code:**
1. Open project in VS Code
2. Click "Build" in PlatformIO toolbar (or Ctrl+Alt+B) - takes 2-3 minutes. NEVER CANCEL.
3. Click "Upload" to flash to device (or Ctrl+Alt+U) - takes 1-2 minutes. NEVER CANCEL.
4. Click "Serial Monitor" to view debug output (115200 baud)

**IMPORTANT: PlatformIO CLI is NOT available in PATH**
- PlatformIO CLI commands (`pio run`, `platformio run`) will NOT work in terminal
- All builds, uploads, and monitoring MUST be done through VS Code PlatformIO extension UI
- Use the PlatformIO toolbar buttons or keyboard shortcuts exclusively
- Do NOT attempt to run PlatformIO commands in terminal - they will fail

**Expected Build Time**: 2-3 minutes for compilation, 1-2 minutes for upload. NEVER CANCEL builds or uploads.

## Validation

### Hardware Testing Scenarios
After successfully uploading firmware, test these complete scenarios:

1. **Initial Setup Flow**:
   - Power on device
   - Connect to "Aura" WiFi network from phone/laptop
   - Navigate to http://192.168.4.1 in browser
   - Configure WiFi credentials
   - Verify device connects to WiFi and displays weather

2. **Weather Display**:
   - Verify current weather displays with icon and temperature
   - Touch screen to access settings
   - Verify 7-day forecast and hourly forecast views work

3. **Settings Configuration**:
   - Test location search and selection
   - Test brightness adjustment
   - Test language switching (English, Spanish, German, French)
   - Test temperature unit toggle (°C/°F)
   - Test 12/24 hour time format toggle

4. **WiFi Reset**:
   - Test WiFi reset functionality
   - Verify device returns to captive portal mode

### Code Validation
- Always run `pio run` (build) before making changes
- Check Serial Monitor output for runtime errors
- Use `extract_unicode_chars.py` when modifying multilingual strings

## Common Tasks

### Key File Locations
```
Repository Structure:
├── src/
│   ├── main.cpp                 # Main Arduino application
│   ├── extract_unicode_chars.py # Unicode analysis utility
│   ├── icon_*.c                 # Weather icon assets
│   ├── image_*.c                # Weather image assets
│   ├── lv_font_*.c              # LVGL font files
│   └── translations.h           # Multilingual string definitions
├── include/
│   ├── Setup_ESP32_2432S028R_ILI9341.h  # TFT_eSPI configuration
│   └── lv_conf.h                # LVGL configuration (if needed)
├── platformio.ini               # PlatformIO project configuration
```

### Modifying Weather Display
- Main UI creation: `create_ui()` function in src/main.cpp
- Weather data fetching: `fetch_and_update_weather()` function
- Icon selection: `choose_image()` and `choose_icon()` functions

### Adding New Languages
1. Add language enum to `enum Language` in src/translations.h
2. Create new `LocalizedStrings` structure
3. Update language switching logic in settings
4. Run `python3 src/extract_unicode_chars.py src/main.cpp` to get required Unicode characters
5. Regenerate LVGL fonts with new character set if needed

### Modifying Display Configuration
- Edit `include/Setup_ESP32_2432S028R_ILI9341.h` for display driver settings
- Current config: ILI9341_2_DRIVER, 240x320 resolution, specific pin mappings
- Edit `include/lv_conf.h` for LVGL library settings (if needed)
- Current config: 16-bit color depth, ESP32 optimizations

### Font Management
- Custom fonts in `src/lv_font_*.c` files
- Use LVGL Font Converter tool to generate new fonts
- Include Unicode characters: `°¿ÉÊÍÓÜßáäçèéíñóöúûü‐→` (from extract_unicode_chars.py)

## Unicode Character Analysis
Use the included Python utility to analyze Unicode requirements:
```bash
cd /path/to/Aura
python3 src/extract_unicode_chars.py src/main.cpp
```
This identifies all non-ASCII characters needed for LVGL font generation.

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

### Main Components
- **src/main.cpp**: Main application logic, UI, and weather fetching
- **LVGL**: UI framework for ESP32 with touchscreen support
- **TFT_eSPI**: Display driver for ILI9341 screens
- **WiFiManager**: Captive portal for WiFi configuration
- **ArduinoJson**: Weather API response parsing
- **XPT2046_Touchscreen**: Touch input handling

### Key Functions
- `setup()`: Initialize hardware, WiFi, display, and UI
- `loop()`: Main application loop with LVGL updates
- `create_ui()`: Build the main weather display interface
- `fetch_and_update_weather()`: Get weather data from API
- `create_settings_window()`: Settings screen with user preferences

### Weather Data Source
- Uses Open-Meteo API for weather data
- Geocoding API for location search
- No API key required
- Updates every 10 minutes (600000ms)

## Development Guidelines

### Making Changes
1. Always test on actual hardware
2. Use Serial Monitor for debugging
3. Keep builds under partition size limit (huge_app.csv)
4. Test all language variants when modifying UI text
5. Verify touchscreen functionality after UI changes

### Code Style
- Follow existing Arduino/C++ conventions
- Use descriptive function names
- Comment complex LVGL UI creation code
- Keep string literals in LocalizedStrings structures
- **Serial Debug Messages**: Use calm, professional tone for Serial.println() statements. Avoid excessive exclamation points. Prefer "WiFi connected" over "WiFi connected!" and "Setup complete" over "Setup complete!"
- Prefer Log.infoln() for logging instead of Serial.println() for consistency

### Memory Management
- ESP32 has limited RAM - be careful with large images/fonts
- Use PROGMEM for static data when possible
- Monitor Serial output for memory allocation failures

## NATS Logging

The Aura2 project uses NATS for remote logging via the `forecast_nats.cpp` module. This allows centralized collection of log messages from the device to an external NATS server.

### NATS Log Message Format
Log messages are published as JSON to the subject `aura2/logs/<device-id>` with the following structure:
```json
{
  "timestamp": 1706858400,
  "message": "Log message text",
  "message_length": 17
}
```
- **timestamp**: Unix timestamp (seconds since epoch) captured using `time(nullptr)`. NTP is enabled via `configTime()` at startup, ensuring externally-correct timestamps for log message recipients.
- **message**: The log message text
- **message_length**: Character count of the message

### Usage
- Enable NATS by configuring the NATS server URL in settings
- Log messages are published asynchronously via `publish_log_message(const char *message)`
- Only publishes when NATS connection is active (`natsConnected` flag)
- Messages are published to a per-device subject for easy filtering

### NTP Configuration
NTP is automatically configured during WiFi initialization in `main.cpp`:
- Uses `pool.ntp.org` and `time.nist.gov` as NTP servers
- Respects the configured time zone offset for local time operations
- `time(nullptr)` returns Unix timestamp in UTC; local time obtained via `getLocalTime()` when needed

## Troubleshooting Build Issues

### Library Version Conflicts
- Libraries are managed by platformio.ini - modify versions there if needed
- Clear PlatformIO cache: `pio system prune` if needed
- Check for conflicting library dependencies

### Configuration File Issues
- Ensure include/Setup_ESP32_2432S028R_ILI9341.h exists and is correct
- Check that build_flags in platformio.ini include the correct files
- Verify no conflicting definitions between configuration files

### PlatformIO-Specific Issues
- **collect2.exe error**: Usually memory/linking issue - check partition scheme
- **Upload fails**: Check USB connection and board selection
- **Dependencies not installing**: Clear .pio folder and rebuild

### Hardware-Specific Problems
- Only works with ESP32-2432S028R ILI9341 devices
- Different display modules may require Setup file modifications
- Touch calibration may need adjustment for different screens