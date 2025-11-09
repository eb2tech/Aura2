#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include "ui/ui.h"
#include "weather.h"
#include "forecast_widgets.h"

#define XPT2046_IRQ 36  // T_IRQ
#define XPT2046_MOSI 32 // T_DIN
#define XPT2046_MISO 39 // T_OUT
#define XPT2046_CLK 25  // T_CLK
#define XPT2046_CS 33   // T_CS
#define LCD_BACKLIGHT_PIN 21

// Display configuration - matches EEZ Studio project settings
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;
static lv_color_t buf1[screenWidth * 20]; // 20 lines buffer

// Forecast grid widgets
lv_obj_t *forecast_datetime_label[7];
lv_obj_t *forecast_visibility_image[7];
lv_obj_t *forecast_precip_low_label[7];
lv_obj_t *forecast_temp_label[7];

TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSpi = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
uint16_t touchScreenMinimumX = 200, touchScreenMaximumX = 3700, touchScreenMinimumY = 240, touchScreenMaximumY = 3800;

// Forecast preferences
Preferences preferences;
bool display_seven_day_forecast = true;
String mqttServer = "";
String mqttPassword = "";
uint32_t brightness = 255;
bool use_fahrenheit = true;
float weather_latitude = 29.7604;
float weather_longitude = -95.3698;
String weather_city = "Houston";
String weather_region = "TX";
bool show_24hour_clock = false;
bool dim_at_night = false;
String dim_start_time = "22:00";
String dim_end_time = "06:00";
uint64_t getChipId()
{
  return ESP.getEfuseMac();
}

String getChipIdString()
{
  uint64_t chipid = ESP.getEfuseMac();
  return String((uint32_t)(chipid >> 32), HEX) + String((uint32_t)chipid, HEX);
}

String getDeviceIdentifier()
{
  return "Aura2-" + getChipIdString();
}

void updateClock(lv_timer_t *timer)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  char buf[16];
  int hour = timeinfo.tm_hour % 12;
  if (hour == 0) hour = 12;
  const char *ampm = (timeinfo.tm_hour < 12) ? "am" : "pm";
  snprintf(buf, sizeof(buf), "%d:%02d%s", hour, timeinfo.tm_min, ampm);

  lv_label_set_text(objects.current_time_label, buf);
}

// LVGL log callback
void logPrint(lv_log_level_t level, const char *buf)
{
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

// Display flushing callback - TFT_eSPI implementation
void displayFlush(lv_display_t *display, const lv_area_t *area, uint8_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushPixels((uint16_t *)color_p, w * h);
  tft.endWrite();

  lv_display_flush_ready(display); // Tell LVGL you are ready with the flushing
}

void touchpadRead(lv_indev_t *indev, lv_indev_data_t *data)
{
  if (touchscreen.touched())
  {
    TS_Point p = touchscreen.getPoint();
    // Some very basic auto calibration so it doesn't go out of range
    if (p.x < touchScreenMinimumX)
      touchScreenMinimumX = p.x;
    if (p.x > touchScreenMaximumX)
      touchScreenMaximumX = p.x;
    if (p.y < touchScreenMinimumY)
      touchScreenMinimumY = p.y;
    if (p.y > touchScreenMaximumY)
      touchScreenMaximumY = p.y;
    // Map this to the pixel position
    data->point.x = map(p.x, touchScreenMinimumX, touchScreenMaximumX, 1, screenWidth);  /* Touchscreen X calibration */
    data->point.y = map(p.y, touchScreenMinimumY, touchScreenMaximumY, 1, screenHeight); /* Touchscreen Y calibration */
    data->state = LV_INDEV_STATE_PRESSED;

    Serial.print("Touch x ");
    Serial.print(data->point.x);
    Serial.print(" y ");
    Serial.println(data->point.y);
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

WiFiManager wifiManager;
bool shouldSaveConfig = false;

void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setupWifi()
{
  Serial.println("Connecting to WiFi...");

  WiFiManagerParameter custom_mqtt_server("mqtt_server", "MQTT Server", mqttServer.c_str(), 40);
  WiFiManagerParameter custom_mqtt_password("mqtt_password", "MQTT Password", mqttPassword.c_str(), 40);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_password);

  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.autoConnect(getDeviceIdentifier().c_str());

  if (shouldSaveConfig)
  {
    mqttServer = String(custom_mqtt_server.getValue());
    mqttPassword = String(custom_mqtt_password.getValue());

    Serial.println("Saving config:");
    Serial.println("MQTT Server: " + mqttServer);
    Serial.println("MQTT Password: " + mqttPassword);

    // Save to preferences
    preferences.putString("mqtt_server", mqttServer);
    preferences.putString("mqtt_password", mqttPassword);
  }

  Serial.println("Connected to WiFi: " + WiFi.localIP().toString());
}

void setupUi()
{
  // Initialize EEZ Studio generated UI
  ui_init();

  static const int32_t col_widths[] = {60, 30, 30, 35, LV_GRID_TEMPLATE_LAST};
  static const int32_t row_heights[] = {20, 20, 20, 20, 20, 20, 20, LV_GRID_TEMPLATE_LAST};

  lv_obj_set_style_grid_column_dsc_array(objects.temperature_grid, col_widths, 0);
  lv_obj_set_style_grid_row_dsc_array(objects.temperature_grid, row_heights, 0);
  lv_obj_set_layout(objects.temperature_grid, LV_LAYOUT_GRID);

  // Set up the 7-day/7-hour forecast grid
  for (int row = 0; row < 7; row++)
  {
    // Create date/time label
    int col = 0;
    forecast_datetime_label[row] = lv_label_create(objects.temperature_grid);
    lv_obj_set_style_text_align(forecast_datetime_label[row], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_grid_cell(forecast_datetime_label[row], LV_GRID_ALIGN_CENTER, col, 1, LV_GRID_ALIGN_CENTER, row, 1);

    // Create visibility image
    ++col;
    forecast_visibility_image[row] = lv_img_create(objects.temperature_grid);
    lv_obj_set_grid_cell(forecast_visibility_image[row], LV_GRID_ALIGN_CENTER, col, 1, LV_GRID_ALIGN_CENTER, row, 1);

    // Create precipitation/low label
    ++col;
    forecast_precip_low_label[row] = lv_label_create(objects.temperature_grid);
    lv_obj_set_style_text_align(forecast_precip_low_label[row], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(forecast_precip_low_label[row], lv_color_hex(0xb9ecff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_grid_cell(forecast_precip_low_label[row], LV_GRID_ALIGN_CENTER, col, 1, LV_GRID_ALIGN_CENTER, row, 1);

    // Create temperature label
    ++col;
    forecast_temp_label[row] = lv_label_create(objects.temperature_grid);
    lv_obj_set_style_text_align(forecast_temp_label[row], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_grid_cell(forecast_temp_label[row], LV_GRID_ALIGN_CENTER, col, 1, LV_GRID_ALIGN_CENTER, row, 1);
  }

  auto clock_timer = lv_timer_create(updateClock, 10*1000, NULL); // Update clock every 10 seconds
  auto weather_timer = lv_timer_create(update_weather, 10*60*1000, NULL); // Update weather every 10 minutes

  lv_timer_ready(clock_timer);   // Initial clock update
  lv_timer_ready(weather_timer); // Initial weather update
}

void setupClock()
{
  // Initialize NTP time synchronization
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1); // Central Time Zone (adjust as needed)
  tzset();
  
  Serial.println("Initializing NTP time synchronization...");
  
  // Wait for time to be set
  struct tm timeinfo;
  int retry = 0;
  const int retry_count = 10;
  while (!getLocalTime(&timeinfo) && retry < retry_count) {
    Serial.println("Failed to obtain time, retrying...");
    delay(2000);
    retry++;
  }
  
  if (retry < retry_count) {
    Serial.println("Time synchronized successfully");
    Serial.println(&timeinfo, "Current time: %A, %B %d %Y %H:%M:%S");
  } else {
    Serial.println("Failed to synchronize time after retries");
  }

  updateClock(NULL); // Initial clock update
}

void setupMdns() {
  Serial.println("Setting up mDNS responder...");
  while (!MDNS.begin(getDeviceIdentifier().c_str())) {
    Serial.println("Error setting up MDNS responder...");
    delay(1000);
  }
  Serial.println("mDNS responder started");

  // MDNS.setInstanceName("Aura2 Weather Display");
  MDNS.addService("http", "tcp", 80);
  MDNS.enableArduino();
}

AsyncWebServer server(80);

struct TemplateEntry {
  const char* key;
  String (*handler)();
};

// Template handler functions
String getBrightness() { return String(brightness); }
String getCurrentLat() { return String(weather_latitude, 6); }
String getCurrentLon() { return String(weather_longitude, 6); }
String getWeatherCity() { return weather_city; }
String getWeatherRegion() { return weather_region; }
String getShow24HourClock() { return show_24hour_clock ? "1" : "0"; }
String getUseFahrenheit() { return use_fahrenheit ? "1" : "0"; }
String getDimAtNight() { return dim_at_night ? "checked" : ""; }
String getDimStartTime() { return dim_start_time; }
String getDimEndTime() { return dim_end_time; }

// Static dispatch table
static const TemplateEntry templateTable[] = {
  {"BRIGHTNESS_VALUE", getBrightness},
  {"CURRENT_LAT", getCurrentLat},
  {"CURRENT_LON", getCurrentLon},
  {"WEATHER_CITY", getWeatherCity},
  {"WEATHER_REGION", getWeatherRegion},
  {"CLOCK_24H_CHECKED", getShow24HourClock},
  {"TEMP_F_CHECKED", getUseFahrenheit},
  {"DIM_AT_NIGHT_CHECKED", getDimAtNight},
  {"DIM_START_TIME", getDimStartTime},
  {"DIM_END_TIME", getDimEndTime},
  {nullptr, nullptr} // Sentinel
};

String templateProcessor(const String &var)
{
  for (const auto& entry : templateTable) {
    if (!entry.key) break; // End of table
    if (var.equals(entry.key)) {
      return entry.handler();
    }
  }
  return String();
};

void setupWebserver()
{
  Serial.println("Setting up web server...");

  if (!LittleFS.begin(false))
  {
    Serial.println("LittleFS mount failed!");
    return;
  }
  else
  {
    Serial.println("LittleFS mounted successfully");
    Serial.printf("Total: %d bytes, Used: %d bytes\n", LittleFS.totalBytes(), LittleFS.usedBytes());
  }

  // Verify required files exist
  if (!LittleFS.exists("/index.html"))
  {
    Serial.println("WARNING: /index.html not found");
  }

  // Static files (index.html, etc.)
  server.serveStatic("/", LittleFS, "/");

  // Serve templated HTML with current brightness value
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (LittleFS.exists("/index.html")) {
      
      request->send(LittleFS, "/index.html", "text/html", false, &templateProcessor);
    } else {
      request->send(404, "text/plain", "index.html not found");
    } });

  // Handle brightness updates with POST
  server.on("/setBrightness", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, (const char*)data);
      
      if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }
      
      int brightnessValue = doc["value"];
      brightnessValue = constrain(brightnessValue, 0, 255);
      
      Serial.printf("Setting brightness to: %d\n", brightnessValue);
      
      // Apply brightness to LCD backlight
      analogWrite(LCD_BACKLIGHT_PIN, brightnessValue);
      
      // Save to preferences for persistence
      preferences.putUInt("brightness", brightnessValue);
      brightness = brightnessValue;
      
      request->send(200, "application/json", "{\"status\":\"ok\"}"); });

  // Handle location updates with POST
  server.on("/setLocation", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, (const char*)data);
      
      if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }
      
      if (!doc.containsKey("latitude") || !doc.containsKey("longitude")) {
        request->send(400, "application/json", "{\"error\":\"Missing latitude or longitude\"}");
        return;
      }
      
      float lat = doc["latitude"];
      float lon = doc["longitude"];
      
      // Validate coordinates
      if (lat < -90 || lat > 90 || lon < -180 || lon > 180) {
        request->send(400, "application/json", "{\"error\":\"Invalid coordinates\"}");
        return;
      }
      
      Serial.printf("Setting location to: %.6f, %.6f \n", lat, lon);
      
      weather_latitude = lat;
      weather_longitude = lon;

      // Save to preferences
      preferences.putFloat("weather_lat", lat);
      preferences.putFloat("weather_lon", lon);
      
      update_weather(nullptr);
      
      request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

// Handle clock format updates with POST
server.on("/setClockFormat", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, 
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc.containsKey("format24")) {
      request->send(400, "application/json", "{\"error\":\"Missing format24 field\"}");
      return;
    }
    
    bool format24 = doc["format24"];
    
    Serial.printf("Setting clock format to: %s\n", format24 ? "24h" : "12h");
    
    show_24hour_clock = format24;
    preferences.putBool("show_24hour", format24);
    
    request->send(200, "application/json", "{\"status\":\"ok\"}");
});

// Handle temperature format updates with POST
server.on("/setTempFormat", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc.containsKey("useF")) {
      request->send(400, "application/json", "{\"error\":\"Missing useF field\"}");
      return;
    }
    
    bool useF = doc["useF"];
    
    Serial.printf("Setting temperature format to: %s\n", useF ? "Fahrenheit" : "Celsius");
    
    use_fahrenheit = useF;
    preferences.putBool("use_fahrenheit", useF);
    
    // Trigger weather update to refresh temperature display
    update_weather(nullptr);
    
    request->send(200, "application/json", "{\"status\":\"ok\"}");
});

// Handle dim at night setting with POST
server.on("/setDimAtNight", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc.containsKey("enabled")) {
      request->send(400, "application/json", "{\"error\":\"Missing enabled field\"}");
      return;
    }
    
    bool enabled = doc["enabled"];
    
    Serial.printf("Setting dim at night to: %s\n", enabled ? "enabled" : "disabled");
    
    // You'll need to add this global variable near the top of main.cpp
    // bool dim_at_night = false;
    // dim_at_night = enabled;
    preferences.putBool("dim_at_night", enabled);
    
    request->send(200, "application/json", "{\"status\":\"ok\"}");
});

// Handle dim start time setting with POST
server.on("/setDimStartTime", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc.containsKey("startTime")) {
      request->send(400, "application/json", "{\"error\":\"Missing startTime field\"}");
      return;
    }
    
    String startTime = doc["startTime"].as<String>();
    
    // Validate time format (HH:MM)
    if (startTime.length() != 5 || startTime.charAt(2) != ':') {
      request->send(400, "application/json", "{\"error\":\"Invalid time format\"}");
      return;
    }
    
    Serial.printf("Setting dim start time to: %s\n", startTime.c_str());
    
    // You'll need to add this global variable near the top of main.cpp
    // String dim_start_time = "22:00";
    // dim_start_time = startTime;
    preferences.putString("dim_start_time", startTime);
    
    request->send(200, "application/json", "{\"status\":\"ok\"}");
});

// Handle dim end time setting with POST
server.on("/setDimEndTime", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc.containsKey("endTime")) {
      request->send(400, "application/json", "{\"error\":\"Missing endTime field\"}");
      return;
    }
    
    String endTime = doc["endTime"].as<String>();
    
    // Validate time format (HH:MM)
    if (endTime.length() != 5 || endTime.charAt(2) != ':') {
      request->send(400, "application/json", "{\"error\":\"Invalid time format\"}");
      return;
    }
    
    Serial.printf("Setting dim end time to: %s\n", endTime.c_str());
    
    // You'll need to add this global variable near the top of main.cpp
    // String dim_end_time = "06:00";
    // dim_end_time = endTime;
    preferences.putString("dim_end_time", endTime);
    
    request->send(200, "application/json", "{\"status\":\"ok\"}");
});  

// Handle IP-based location detection with POST
server.on("/detectLocation", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    Serial.println("Detecting location via IP geolocation...");
    
    HTTPClient http;
    http.begin("https://ipapi.co/json/");
    http.addHeader("User-Agent", "Aura-ESP32/1.0");
    http.setTimeout(10000); // 10 second timeout
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode == 200) {
      String payload = http.getString();
      Serial.printf("Geolocation response: %s\n", payload.c_str());
      
      // Parse the response
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error && doc.containsKey("latitude") && doc.containsKey("longitude")) {
        float lat = doc["latitude"];
        float lon = doc["longitude"];
        String city = doc["city"] | "Unknown";
        String region = doc["region"] | "Unknown";
        
        // Validate coordinates
        if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
          Serial.printf("Location detected: %.6f, %.6f (%s, %s)\n", lat, lon, city.c_str(), region.c_str());
          
          // Save to preferences
          preferences.putFloat("weather_lat", lat);
          preferences.putFloat("weather_lon", lon);
          preferences.putString("weather_city", city);
          preferences.putString("weather_region", region);
          
          // Trigger weather update with new location
          update_weather(nullptr);
          
          // Return success with location data
          JsonDocument response;
          response["status"] = "ok";
          response["latitude"] = lat;
          response["longitude"] = lon;
          response["city"] = city;
          response["region"] = region;
          
          String responseStr;
          serializeJson(response, responseStr);
          request->send(200, "application/json", responseStr);
        } else {
          request->send(400, "application/json", "{\"error\":\"Invalid coordinates received\"}");
        }
      } else {
        request->send(500, "application/json", "{\"error\":\"Failed to parse location data\"}");
      }
    } else {
      Serial.printf("HTTP error: %d\n", httpResponseCode);
      
      // Check for redirect response
      if (httpResponseCode == 301 || httpResponseCode == 302) {
        String location = http.getLocation();
        Serial.printf("Redirect to: %s\n", location.c_str());
        request->send(500, "application/json", "{\"error\":\"Service moved - please update firmware\"}");
      } else {
        request->send(500, "application/json", "{\"error\":\"Geolocation service unavailable\"}");
      }
    }
    
    http.end();
});
  server.begin();
  Serial.println("Web server started on port 80");
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Aura2 Starting...");

  String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);

  Serial.println("In setup()");

  // Initialize TFT display hardware
  tft.init();
  tft.setRotation(2);
  pinMode(LCD_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(LCD_BACKLIGHT_PIN, HIGH); // Turn on backlight

  // Initialize the touchscreen
  touchscreenSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS); // Start second SPI bus for touchscreen
  touchscreen.begin(touchscreenSpi);                                         // Touchscreen init
  touchscreen.setRotation(1);                                                // Inverted landscape orientation to match screen

  // Load saved preferences
  preferences.begin("aura2", false);
  display_seven_day_forecast = preferences.getBool("display_7day", display_seven_day_forecast);
  mqttServer = preferences.getString("mqtt_server", mqttServer);
  mqttPassword = preferences.getString("mqtt_password", mqttPassword);
  brightness = preferences.getUInt("brightness", brightness);
  use_fahrenheit = preferences.getBool("use_fahrenheit", use_fahrenheit);
  weather_latitude = preferences.getFloat("weather_lat", weather_latitude);
  weather_longitude = preferences.getFloat("weather_lon", weather_longitude);
  weather_city = preferences.getString("weather_city", weather_city);
  weather_region = preferences.getString("weather_region", weather_region);
  show_24hour_clock = preferences.getBool("show_24hour", show_24hour_clock);
  dim_at_night = preferences.getBool("dim_at_night", dim_at_night);
  dim_start_time = preferences.getString("dim_start_time", dim_start_time);
  dim_end_time = preferences.getString("dim_end_time", dim_end_time);

  // Initialize LVGL
  lv_init();
  lv_log_register_print_cb(logPrint);

  // Create display (LVGL 9.x API)
  lv_display_t *display = lv_display_create(screenWidth, screenHeight);

  // Set display buffer with double buffering for smoother refresh
  lv_display_set_buffers(display, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  // Set display flush callback
  lv_display_set_flush_cb(display, displayFlush);

  // Set up touch input device
  lv_indev_t *indev_touchpad = lv_indev_create();
  lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev_touchpad, touchpadRead);

  // Set up everything else
  setupWifi();
  setupMdns();
  setupUi();
  setupClock();
  setupWebserver();

  Serial.println("UI initialized and ready!");

  // Force initial screen refresh
  lv_refr_now(display);

  Serial.println("Setup complete!");
}

void loop()
{
  // CRITICAL: Tell LVGL how much time has passed
  lv_tick_inc(3); // 3ms matches our delay below for faster refresh

  // Handle LVGL tasks
  lv_timer_handler();

  // Handle EEZ Studio UI updates
  ui_tick();

  // Small delay to prevent watchdog issues - reduced for faster refresh
  delay(3);
}
