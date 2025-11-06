#pragma once

#include <lvgl.h>
#include <Preferences.h>

// External references to forecast grid widgets defined in main.cpp
extern lv_obj_t *forecast_datetime_label[7];
extern lv_obj_t *forecast_visibility_image[7];
extern lv_obj_t *forecast_precip_low_label[7];
extern lv_obj_t *forecast_temp_label[7];

extern Preferences preferences;