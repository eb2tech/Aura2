#include <Arduino.h>
#include "ui/actions.h"
#include "ui/ui.h"
#include "forecast_weather.h"
#include "forecast_settings.h"

extern "C" void action_toggle_forecast(lv_event_t *e) {
    toggleSevenDayForecast();
}

extern "C" void action_navigate_home(lv_event_t *e) {
    loadScreen(SCREEN_ID_MAIN);
}

extern "C" void action_navigate_settings(lv_event_t *e) {
    loadScreen(SCREEN_ID_SETTINGS);
}

extern "C" void action_init_settings(lv_event_t *e) {
    initializeSettingsScreen();
}
