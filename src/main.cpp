#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include "ui/ui.h"
#include "secrets.h"
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

// WiFi credentials
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

WiFiClient espClient;

TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSpi = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
uint16_t touchScreenMinimumX = 200, touchScreenMaximumX = 3700, touchScreenMinimumY = 240, touchScreenMaximumY = 3800;
Preferences preferences;

void update_clock(lv_timer_t *timer)
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
void log_print(lv_log_level_t level, const char *buf)
{
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

// Display flushing callback - TFT_eSPI implementation
void disp_flush(lv_display_t *display, const lv_area_t *area, uint8_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushPixels((uint16_t *)color_p, w * h);
  tft.endWrite();

  lv_display_flush_ready(display); // Tell LVGL you are ready with the flushing
}

void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
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

void setup_wifi()
{
  delay(10);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("  Connecting...");
  }
  Serial.println("Connected to WiFi: " + WiFi.localIP().toString());
}

void setup_ui()
{
  // Initialize EEZ Studio generated UI
  ui_init();

  static const int32_t col_widths[] = {60, 30, 30, 35, LV_GRID_TEMPLATE_LAST};
  static const int32_t row_heights[] = {20, 20, 20, 20, 20, 20, 20, LV_GRID_TEMPLATE_LAST};

  lv_obj_set_style_grid_column_dsc_array(objects.temperature_grid, col_widths, 0);
  lv_obj_set_style_grid_row_dsc_array(objects.temperature_grid, row_heights, 0);
  lv_obj_set_layout(objects.temperature_grid, LV_LAYOUT_GRID);

  // Set up the 7-day forecast grid
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

  auto clock_timer = lv_timer_create(update_clock, 30*1000, NULL); // Update clock every 30 seconds
  auto weather_timer = lv_timer_create(update_weather, 10*60*1000, NULL); // Update weather every 10 minutes

  lv_timer_ready(clock_timer);   // Initial clock update
  lv_timer_ready(weather_timer); // Initial weather update
}

void setup_clock()
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

  update_clock(NULL); // Initial clock update
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
  display_seven_day_forecast = preferences.getBool("display_7day", true);

  // Initialize LVGL
  lv_init();
  lv_log_register_print_cb(log_print);

  // Create display (LVGL 9.x API)
  lv_display_t *display = lv_display_create(screenWidth, screenHeight);

  // Set display buffer with double buffering for smoother refresh
  lv_display_set_buffers(display, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  // Set display flush callback
  lv_display_set_flush_cb(display, disp_flush);

  // Set up touch input device
  lv_indev_t *indev_touchpad = lv_indev_create();
  lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev_touchpad, touchpad_read);

  // Set up everything else
  setup_wifi();
  setup_ui();
  setup_clock();

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
