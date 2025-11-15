#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "ui/ui.h"
#include "forecast_preferences.h"
#include "forecast_settings.h"
#include "forecast_weather.h"
#include "forecast_widgets.h"
#include "forecast_mqtt.h"

#define LCD_BACKLIGHT_PIN 21

void initializeSettingsScreen()
{
    Serial.println("Initializing settings screen...");

    String ipv4LabelText = "http://" + WiFi.localIP().toString();
    String mdnsLabelText = "http://" + getDeviceIdentifier() + ".local";

    lv_label_set_text(objects.ipv4_label, ipv4LabelText.c_str());
    lv_label_set_text(objects.mdns_label, mdnsLabelText.c_str());
}

AsyncWebServer server(80);

struct TemplateEntry
{
    const char *key;
    String (*handler)();
};

// Template handler functions
String getBrightness() { return String(brightness); }
String getCurrentLat() { return String(weather_latitude, 6); }
String getCurrentLon() { return String(weather_longitude, 6); }
String getWeatherCity() { return weather_city; }
String getWeatherRegion() { return weather_region; }
String getShow24HourClock() { return show_24hour_clock ? "checked" : ""; }
String getUseFahrenheit() { return use_fahrenheit ? "checked" : ""; }
String getDimAtTime() { return dim_at_time ? "checked" : ""; }
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
    {"DIM_AT_TIME_CHECKED", getDimAtTime},
    {"DIM_START_TIME", getDimStartTime},
    {"DIM_END_TIME", getDimEndTime},
    {nullptr, nullptr} // Sentinel
};

String templateProcessor(const String &var)
{
    for (const auto &entry : templateTable)
    {
        if (!entry.key)
            break; // End of table
        if (var.equals(entry.key))
        {
            return entry.handler();
        }
    }
    return String();
};

void setupWebserver()
{
    Serial.println("Setting up settings web server...");
    
    if (!LittleFS.begin(false))
    {
        Serial.println("LittleFS mount failed");
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
      publishBacklightState();
      
      request->send(200, "application/json", "{\"status\":\"ok\"}"); });

    // Handle location updates with POST
    server.on("/setLocation", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, (const char*)data);
      
      if (error) {
        request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }
      
      if (!doc["latitude"].is<float>() || !doc["longitude"].is<float>()) {
        request->send(400, "application/json", "{\"error\":\"Latitude and longitude must be float values\"}");
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
      
      updateWeather(nullptr);
      
      request->send(200, "application/json", "{\"status\":\"ok\"}"); });

    // Handle clock format updates with POST
    server.on("/setClockFormat", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc["format24"].is<bool>()) {
      request->send(400, "application/json", "{\"error\":\"Missing format24 field\"}");
      return;
    }
    
    bool format24 = doc["format24"];
    
    Serial.printf("Setting clock format to: %s\n", format24 ? "24h" : "12h");
    
    show_24hour_clock = format24;
    preferences.putBool("show_24hour", format24);

    // Trigger clock update to refresh display
    updateClock(nullptr);
    
    request->send(200, "application/json", "{\"status\":\"ok\"}"); });

    // Handle temperature format updates with POST
    server.on("/setTempFormat", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc["useF"].is<bool>()) {
      request->send(400, "application/json", "{\"error\":\"useF must be a boolean value\"}");
      return;
    }

    bool useF = doc["useF"];
    
    Serial.printf("Setting temperature format to: %s\n", useF ? "Fahrenheit" : "Celsius");
    
    use_fahrenheit = useF;
    preferences.putBool("use_fahrenheit", useF);
    
    // Trigger weather update to refresh temperature display
    updateWeather(nullptr);
    
    request->send(200, "application/json", "{\"status\":\"ok\"}"); });

    // Handle dim at time setting with POST
    server.on("/setDimAtTime", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc["enabled"].is<bool>()) {
      request->send(400, "application/json", "{\"error\":\"enabled must be a boolean value\"}");
      return;
    }
    
    bool enabled = doc["enabled"];
    
    Serial.printf("Setting dim at time to: %s\n", enabled ? "enabled" : "disabled");
    
    dim_at_time = enabled;
    preferences.putBool("dim_at_time", enabled);
    checkDimTime(nullptr);
    
    request->send(200, "application/json", "{\"status\":\"ok\"}"); });

    // Handle dim start time setting with POST
    server.on("/setDimStartTime", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc["startTime"].is<String>()) {
      request->send(400, "application/json", "{\"error\":\"startTime must be a string value\"}");
      return;
    }
    
    String startTime = doc["startTime"].as<String>();
    
    // Validate time format (HH:MM)
    if (startTime.length() != 5 || startTime.charAt(2) != ':') {
      request->send(400, "application/json", "{\"error\":\"Invalid time format\"}");
      return;
    }
    
    Serial.printf("Setting dim start time to: %s\n", startTime.c_str());
    
    dim_start_time = startTime;
    preferences.putString("dim_start_time", startTime);
    checkDimTime(nullptr);
    
    request->send(200, "application/json", "{\"status\":\"ok\"}"); });

    // Handle dim end time setting with POST
    server.on("/setDimEndTime", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    
    if (error) {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc["endTime"].is<String>()) {
      request->send(400, "application/json", "{\"error\":\"endTime must be a string value\"}");
      return;
    }
    
    String endTime = doc["endTime"].as<String>();
    
    // Validate time format (HH:MM)
    if (endTime.length() != 5 || endTime.charAt(2) != ':') {
      request->send(400, "application/json", "{\"error\":\"Invalid time format\"}");
      return;
    }
    
    Serial.printf("Setting dim end time to: %s\n", endTime.c_str());
    
    dim_end_time = endTime;
    preferences.putString("dim_end_time", endTime);
    
    request->send(200, "application/json", "{\"status\":\"ok\"}"); });

    // Handle IP-based location detection with POST
    server.on("/detectLocation", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
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
      
      if (!error && doc["latitude"].is<float>() && doc["longitude"].is<float>()) {
        float lat = doc["latitude"];
        float lon = doc["longitude"];
        String city = doc["city"] | "Unknown";
        String region = doc["region"] | "Unknown";
        
        // Validate coordinates
        if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
          Serial.printf("Location detected: %.6f, %.6f (%s, %s)\n", lat, lon, city.c_str(), region.c_str());
          
          // Update global variables
          weather_latitude = lat;
          weather_longitude = lon;
          weather_city = city;
          weather_region = region;

          // Save to preferences
          preferences.putFloat("weather_lat", lat);
          preferences.putFloat("weather_lon", lon);
          preferences.putString("weather_city", city);
          preferences.putString("weather_region", region);
          
          // Trigger weather update with new location
          updateWeather(nullptr);
          
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
    
    http.end(); });

    server.begin();
    Serial.println("Web server started on port 80");
}
