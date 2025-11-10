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
#include "ui/ui.h"
#include "forecast_weather.h"
#include "forecast_widgets.h"
#include "forecast_settings.h"

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
bool dim_at_time = false;
String dim_start_time = "22:00";
String dim_end_time = "06:00";
bool dimModeActive = false;

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

bool itsDimTime()
{
  if (!dim_at_time)
    return false;

  auto timeToMinutes = [](const String &timeStr)
  {
    int hour = timeStr.substring(0, 2).toInt();
    int minute = timeStr.substring(3, 5).toInt();
    return hour * 60 + minute;
  };

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
    return false;

  int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int startMinutes = timeToMinutes(dim_start_time);
  int endMinutes = timeToMinutes(dim_end_time);

  if (startMinutes < endMinutes)
  {
    return (currentMinutes >= startMinutes) && (currentMinutes < endMinutes);
  }
  else
  {
    return (currentMinutes >= startMinutes) || (currentMinutes < endMinutes);
  }
}

void checkDimTime(lv_timer_t *timer)
{
  auto goDim = []()
  {
    analogWrite(LCD_BACKLIGHT_PIN, 0);
    dimModeActive = true;
  };
  auto restoreBrightness = [=]()
  {
    analogWrite(LCD_BACKLIGHT_PIN, brightness);
    dimModeActive = false;
  };

  bool dimTime = itsDimTime();
  if (dimTime && !dimModeActive)
  {
    goDim();
  }
  else if (!dimTime && dimModeActive)
  {
    restoreBrightness();
  }
}

void showWiFiSplashScreen()
{
  // Clear screen with black background
  tft.fillScreen(TFT_BLACK);

  // Set text properties
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextWrap(true);
  tft.setTextSize(1);

  // Title
  tft.setTextSize(2);
  tft.setCursor(30, 20);
  tft.println("Aura2 Setup");

  // Device ID
  tft.setTextSize(1);
  tft.setCursor(10, 55);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println("Device: " + getDeviceIdentifier());

  // Main instructions
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 80);
  tft.println("WiFi configuration needed:");

  tft.setCursor(10, 100);
  tft.println("1. Connect phone/laptop to:");

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(20, 115);
  tft.println("   \"" + getDeviceIdentifier() + "\"");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 135);
  tft.println("2. Open web browser");

  tft.setCursor(10, 150);
  tft.println("3. Go to: ");
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.print("192.168.4.1");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 170);
  tft.println("4. Select your WiFi network");

  tft.setCursor(10, 185);
  tft.println("5. Enter password");

  // Status at bottom
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(10, 210);
  tft.println("Starting WiFi hotspot...");

  // Force display update
  delay(100);
}

void updateWiFiSplashStatus(const String &status, uint16_t color = TFT_GREEN)
{
  // Clear status area
  tft.fillRect(10, 210, 300, 20, TFT_BLACK);

  // Update status
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 210);
  tft.println(status);

  delay(100);
}

void updateClock(lv_timer_t *timer)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
    return;

  char buf[16];

  if (show_24hour_clock)
  {
    snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  }
  else
  {
    int hour = timeinfo.tm_hour % 12;
    if (hour == 0)
      hour = 12;
    const char *ampm = (timeinfo.tm_hour < 12) ? "am" : "pm";
    snprintf(buf, sizeof(buf), "%d:%02d%s", hour, timeinfo.tm_min, ampm);
  }

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

// WiFiManager callback functions for splash screen updates
void onWiFiManagerAPStarted(WiFiManager *wifiManager)
{
  updateWiFiSplashStatus("Hotspot ready! Connect now", TFT_YELLOW);
  Serial.println("AP Mode started");
}

void onWiFiManagerConnected()
{
  updateWiFiSplashStatus("WiFi connected! Loading...", TFT_GREEN);
  Serial.println("WiFi connected via WiFiManager");
}

void setupWifi()
{
  Serial.println("Connecting to WiFi...");

  // Check if already connected
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Already connected to WiFi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return;
  }

  // Show splash screen for configuration
  showWiFiSplashScreen();

  WiFiManagerParameter custom_mqtt_server("mqtt_server", "MQTT Server", mqttServer.c_str(), 40);
  WiFiManagerParameter custom_mqtt_password("mqtt_password", "MQTT Password", mqttPassword.c_str(), 40);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_password);

  // Set callbacks
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setAPCallback(onWiFiManagerAPStarted);

  // Set timeout for captive portal (5 minutes)
  wifiManager.setConfigPortalTimeout(300);

  // Update status before starting
  updateWiFiSplashStatus("Checking saved WiFi...", TFT_CYAN);

  // Try to connect, if it fails start captive portal
  if (!wifiManager.autoConnect(getDeviceIdentifier().c_str()))
  {
    updateWiFiSplashStatus("WiFi setup timeout!", TFT_RED);
    Serial.println("Failed to connect and hit timeout");
    delay(3000);
    // Reset and try again
    ESP.restart();
  }
  else
  {
    // Connected successfully
    updateWiFiSplashStatus("WiFi connected successfully!", TFT_GREEN);
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    delay(2000); // Show success message briefly
  }

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

  // Because LVGL timers are used, we set up periodic update timers here
  auto clock_timer = lv_timer_create(updateClock, 10 * 1000, NULL);          // Update clock every 10 seconds
  auto weather_timer = lv_timer_create(updateWeather, 10 * 60 * 1000, NULL); // Update weather every 10 minutes
  auto dim_timer = lv_timer_create(checkDimTime, 1 * 60 * 1000, NULL);      // Check dim time every minute

  lv_timer_ready(clock_timer);   // Initial clock update
  lv_timer_ready(weather_timer); // Initial weather update
  lv_timer_ready(dim_timer);     // Initial dim time check
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
  while (!getLocalTime(&timeinfo) && retry < retry_count)
  {
    Serial.println("Failed to obtain time, retrying...");
    delay(2000);
    retry++;
  }

  if (retry < retry_count)
  {
    Serial.println("Time synchronized successfully");
    Serial.println(&timeinfo, "Current time: %A, %B %d %Y %H:%M:%S");
  }
  else
  {
    Serial.println("Failed to synchronize time after retries");
  }

  updateClock(NULL); // Initial clock update
}

void setupMdns()
{
  Serial.println("Setting up mDNS responder...");
  while (!MDNS.begin(getDeviceIdentifier().c_str()))
  {
    Serial.println("Error setting up MDNS responder...");
    delay(1000);
  }
  Serial.println("mDNS responder started");

  MDNS.addService("http", "tcp", 80);
  MDNS.enableArduino();
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
  dim_at_time = preferences.getBool("dim_at_time", dim_at_time);
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

  Serial.println("UI initialized and ready");

  // Force initial screen refresh
  lv_refr_now(display);

  Serial.println("Setup complete");
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
