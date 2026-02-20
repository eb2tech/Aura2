#pragma once

#include "lvgl.h"

// Weather Icons and Images are loaded from SPIFFS to save flash memory
const char *chooseImage(int code, int is_day);
const char *chooseIcon(int code, int is_day);

extern bool display_seven_day_forecast;
extern float temperature_now;
extern float feels_like_temperature;

void updateWeather(lv_timer_t *timer);
void toggleSevenDayForecast();
