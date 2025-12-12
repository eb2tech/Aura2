#pragma once

#include <Preferences.h>

extern Preferences preferences;
extern bool display_seven_day_forecast;
extern String mqttServer;
extern String mqttUser;
extern String mqttPassword;
extern uint32_t brightness;
extern bool use_fahrenheit;
extern float weather_latitude;
extern float weather_longitude;
extern String weather_city;
extern String weather_region;
extern bool show_24hour_clock;
extern bool dim_at_time;
extern String dim_start_time;  
extern String dim_end_time;
extern String time_zone;
extern String utc_offset;
extern bool use_dst;
extern bool use_mqtt;

String getDeviceIdentifier();
