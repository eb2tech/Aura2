#pragma once

#include <lvgl.h>

// External references to forecast grid widgets defined in main.cpp
extern lv_obj_t *forecast_datetime_label[7];
extern lv_obj_t *forecast_visibility_image[7];
extern lv_obj_t *forecast_precip_low_label[7];
extern lv_obj_t *forecast_temp_label[7];

void updateClock(lv_timer_t *timer);
void checkDimTime(lv_timer_t *timer);

struct backlightState_t
{
    bool isOn;
    uint8_t brightness; // 0-255
};

backlightState_t getBacklightState();

void logHeapStats(const char *context);