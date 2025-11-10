#include <Arduino.h>
#include "ui/actions.h"
#include "forecast_weather.h"

extern "C" void action_toggle_forecast(lv_event_t *e) {
    toggleSevenDayForecast();
}
