#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include "ui/ui.h"
#include "forecast_preferences.h"
#include "forecast_settings.h"
#include "forecast_weather.h"
#include "forecast_widgets.h"
#include "forecast_mqtt.h"

#define LCD_BACKLIGHT_PIN 21

void initializeSettingsScreen()
{
  Log.infoln("Initializing settings screen...");
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
String getUseDST() { return use_dst ? "checked" : ""; }
String getMqttUsername() { return mqttUser; }
String getMqttPassword() { return mqttPassword; }
String getUseMQTT() { return use_mqtt ? "checked" : ""; }

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
    {"USE_DST_CHECKED", getUseDST},
    {"MQTT_USERNAME", getMqttUsername},
    {"MQTT_PASSWORD", getMqttPassword},
    {"USE_MQTT_CHECKED", getUseMQTT},
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
    Log.errorln("LittleFS mount failed");
    return;
  }
  else
  {
    Log.infoln("LittleFS mounted successfully");
    Log.infoln("Total: %d bytes, Used: %d bytes", LittleFS.totalBytes(), LittleFS.usedBytes());
  }

  // Verify required files exist
  if (!LittleFS.exists("/index.html"))
  {
    Log.warningln("WARNING: /index.html not found");
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
        Log.error("JSON parse error: ");
        Log.errorln(error.c_str());
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

  // Handle clock format updates with POST
  server.on("/setClockFormat", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    
    if (error) {
      Log.error("JSON parse error: ");
      Log.errorln(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc["format24"].is<bool>()) {
      request->send(400, "application/json", "{\"error\":\"Missing format24 field\"}");
      return;
    }
    
    bool format24 = doc["format24"];
    
    Log.infoln("Setting clock format to: %s", format24 ? "24h" : "12h");
    
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
      Log.error("JSON parse error: ");
      Log.errorln(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc["useF"].is<bool>()) {
      request->send(400, "application/json", "{\"error\":\"useF must be a boolean value\"}");
      return;
    }

    bool useF = doc["useF"];
    
    Log.infoln("Setting temperature format to: %s", useF ? "Fahrenheit" : "Celsius");
    
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
      Log.error("JSON parse error: ");
      Log.errorln(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc["enabled"].is<bool>()) {
      request->send(400, "application/json", "{\"error\":\"enabled must be a boolean value\"}");
      return;
    }
    
    bool enabled = doc["enabled"];
    
    Log.infoln("Setting dim at time to: %s", enabled ? "enabled" : "disabled");
    
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
      Log.error("JSON parse error: ");
      Log.errorln(error.c_str());
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
    
    Log.infoln("Setting dim start time to: %s", startTime.c_str());
    
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
      Log.error("JSON parse error: ");
      Log.errorln(error.c_str());
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
    
    Log.infoln("Setting dim end time to: %s", endTime.c_str());
    
    dim_end_time = endTime;
    preferences.putString("dim_end_time", endTime);
    
    request->send(200, "application/json", "{\"status\":\"ok\"}"); });

  // Handle DST setting with POST
  server.on("/setUseDST", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    if (error) {
      Log.error("JSON parse error: ");
      Log.errorln(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc["enabled"].is<bool>()) {
      request->send(400, "application/json", "{\"error\":\"enabled must be a boolean value\"}");
      return;
    }
    
    bool enabled = doc["enabled"];
    
    Log.infoln("Setting use DST to: %s", enabled ? "enabled" : "disabled");
    
    use_dst = enabled;
    preferences.putBool("use_dst", enabled);
    updateClock(nullptr);
    
    request->send(200, "application/json", "{\"status\":\"ok\"}"); });

  // Handle MQTT setting with POST
  server.on("/setUseMQTT", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    if (error) {
      Log.error("JSON parse error: ");
      Log.errorln(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc["enabled"].is<bool>()) {
      request->send(400, "application/json", "{\"error\":\"enabled must be a boolean value\"}");
      return;
    }
    
    bool enabled = doc["enabled"];
    
    Log.infoln("Setting use MQTT to: %s", enabled ? "enabled" : "disabled");
    
    use_mqtt = enabled;
    preferences.putBool("use_mqtt", enabled);
    checkMqttConnection(nullptr);
    
    request->send(200, "application/json", "{\"status\":\"ok\"}"); });

  // Handle MQTT username and password settings with POST
  server.on("/setMqttUsername", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    if (error) {
      Log.error("JSON parse error: ");
      Log.errorln(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    if (!doc["username"].is<String>()) {
      request->send(400, "application/json", "{\"error\":\"username and password must be string values\"}");
      return;
    }

    String username = doc["username"].as<String>();

    Log.infoln("Setting MQTT username: %s", username.c_str());

    mqttUser = username;
    preferences.putString("mqtt_user", username);
    checkMqttConnection(nullptr);

    request->send(200, "application/json", "{\"status\":\"ok\"}"); });

  // Handle MQTT username and password settings with POST
  server.on("/setMqttPassword", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    if (error) {
      Log.error("JSON parse error: ");
      Log.errorln(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    if (!doc["password"].is<String>()) {
      request->send(400, "application/json", "{\"error\":\"username and password must be string values\"}");
      return;
    }

    String password = doc["password"].as<String>();

    Log.infoln("Setting MQTT password.");

    mqttPassword = password;
    preferences.putString("mqtt_password", password);
    checkMqttConnection(nullptr);

    request->send(200, "application/json", "{\"status\":\"ok\"}"); });

  // Handle location updates with POST
  server.on("/setLocation", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, (const char*)data);
    
    if (error) {
      Log.error("JSON parse error: ");
      Log.errorln(error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    if (!doc["latitude"].is<float>() || !doc["longitude"].is<float>()) {
      Log.errorln("Latitude and longitude must be float values");
      request->send(400, "application/json", "{\"error\":\"Latitude and longitude must be float values\"}");
      return;
    }

    float lat = doc["latitude"].as<float>();
    float lon = doc["longitude"].as<float>();
    
    // Validate coordinates
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180) {
      Log.errorln("Invalid coordinates");
      request->send(400, "application/json", "{\"error\":\"Invalid coordinates\"}");
      return;
    }
    
    Log.infoln("Setting location to: %.6f, %.6f", lat, lon);
    
    weather_latitude = lat;
    weather_longitude = lon;

    // Save to preferences
    preferences.putFloat("weather_lat", lat);
    preferences.putFloat("weather_lon", lon);

    Serial.println("Performing reverse geocoding...");
    
    HTTPClient http;
    String url = String("https://api.bigdatacloud.net/data/reverse-geocode-client?latitude=") + String(lat) + "&longitude=" + String(lon) + "&localityLanguage=en";
    http.begin(url);
    http.addHeader("User-Agent", "Aura-ESP32/1.0");
    http.setTimeout(10000); // 10 second timeout

    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
      String payload = http.getString();
      Log.infoln("Reverse geocode response: %s", payload.c_str());
      
      // Parse the response
      JsonDocument geoDoc;
      DeserializationError geoError = deserializeJson(geoDoc, payload);
      if (!geoError) {
        weather_city = geoDoc["city"] | weather_city;
        weather_region = geoDoc["principalSubdivision"] | weather_region;
        preferences.putString("weather_city", weather_city);
        preferences.putString("weather_region", weather_region);

        Log.infoln("Resolved location: %s, %s", weather_city.c_str(), weather_region.c_str());
      } else {
        Log.error("Reverse geocode JSON parse error: ");
        Log.errorln(geoError.c_str());
      }
    } else {
      Log.error("Reverse geocode HTTP error: ");
      Log.errorln(String(httpResponseCode).c_str());
    }
    http.end();

    updateWeather(nullptr);
    
    request->send(200, "application/json", "{\"status\":\"ok\"}"); });

  // Handle IP-based location detection with POST
  server.on("/detectLocation", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
    Log.infoln("Detecting location via IP geolocation...");
    
    HTTPClient http;
    http.begin("https://ipapi.co/json/");
    http.addHeader("User-Agent", "Aura-ESP32/1.0");
    http.setTimeout(10000); // 10 second timeout
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode == 200) {
      String payload = http.getString();
      Log.infoln("Geolocation response: %s", payload.c_str());
      
      // Parse the response
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error && doc["latitude"].is<float>() && doc["longitude"].is<float>()) {
        float lat = doc["latitude"];
        float lon = doc["longitude"];
        String city = doc["city"] | "Unknown";
        String region = doc["region"] | "Unknown";
        String timeZone = doc["timezone"] | "UTC";
        String utcOffset = doc["utc_offset"] | "+00:00";
        
        // Validate coordinates
        if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
          Serial.printf("Location detected: %.6f, %.6f (%s, %s)\n", lat, lon, city.c_str(), region.c_str());
          
          // Update global variables
          weather_latitude = lat;
          weather_longitude = lon;
          weather_city = city;
          weather_region = region;
          time_zone = timeZone;
          utc_offset = utcOffset;

          // Save to preferences
          preferences.putFloat("weather_lat", lat);
          preferences.putFloat("weather_lon", lon);
          preferences.putString("weather_city", city);
          preferences.putString("weather_region", region);
          preferences.putString("time_zone", timeZone);
          preferences.putString("utc_offset", utcOffset);
          
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
      Log.errorln("HTTP error: %d", httpResponseCode);
      
      // Check for redirect response
      if (httpResponseCode == 301 || httpResponseCode == 302) {
        String location = http.getLocation();
        Log.infoln("Redirect to: %s", location.c_str());
        request->send(500, "application/json", "{\"error\":\"Service moved - please update firmware\"}");
      } else {
        request->send(500, "application/json", "{\"error\":\"Geolocation service unavailable\"}");
      }
    }
    
    http.end(); });

  server.begin();
  Log.infoln("Web server started on port 80");
}
