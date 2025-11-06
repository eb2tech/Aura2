#include <Arduino.h>
#include "ui/actions.h"
#include "weather.h"

extern "C" void action_toggle_forecast(lv_event_t *e) {
    toggle_seven_day_forecast();
}
