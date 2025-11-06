#include <lvgl.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "text_strings.h"
#include "weather.h"
#include "forecast_widgets.h"
#include "ui/ui.h"

#define LATITUDE_DEFAULT "29.7604"
#define LONGITUDE_DEFAULT "-95.3698"

static char latitude[16] = LATITUDE_DEFAULT;
static char longitude[16] = LONGITUDE_DEFAULT;

bool display_seven_day_forecast = true;

const lv_img_dsc_t *choose_image(int code, int is_day)
{
    switch (code)
    {
    // Clear sky
    case 0:
        return is_day
                   ? &image_sunny
                   : &image_clear_night;

    // Mainly clear
    case 1:
        return is_day
                   ? &image_mostly_sunny
                   : &image_mostly_clear_night;

    // Partly cloudy
    case 2:
        return is_day
                   ? &image_partly_cloudy
                   : &image_partly_cloudy_night;

    // Overcast
    case 3:
        return &image_cloudy;

    // Fog / mist
    case 45:
    case 48:
        return &image_haze_fog_dust_smoke;

    // Drizzle (light → dense)
    case 51:
    case 53:
    case 55:
        return &image_drizzle;

    // Freezing drizzle
    case 56:
    case 57:
        return &image_sleet_hail;

    // Rain: slight showers
    case 61:
        return is_day
                   ? &image_scattered_showers_day
                   : &image_scattered_showers_night;

    // Rain: moderate
    case 63:
        return &image_showers_rain;

    // Rain: heavy
    case 65:
        return &image_heavy_rain;

    // Freezing rain
    case 66:
    case 67:
        return &image_wintry_mix_rain_snow;

    // Snow fall (light, moderate, heavy) & snow showers (light)
    case 71:
    case 73:
    case 75:
    case 85:
        return &image_snow_showers_snow;

    // Snow grains
    case 77:
        return &image_flurries;

    // Rain showers (slight → moderate)
    case 80:
    case 81:
        return is_day
                   ? &image_scattered_showers_day
                   : &image_scattered_showers_night;

    // Rain showers: violent
    case 82:
        return &image_heavy_rain;

    // Heavy snow showers
    case 86:
        return &image_heavy_snow;

    // Thunderstorm (light)
    case 95:
        return is_day
                   ? &image_isolated_scattered_tstorms_day
                   : &image_isolated_scattered_tstorms_night;

    // Thunderstorm with hail
    case 96:
    case 99:
        return &image_strong_tstorms;

    // Fallback for any other code
    default:
        return is_day
                   ? &image_mostly_cloudy_day
                   : &image_mostly_cloudy_night;
    }
}

const lv_img_dsc_t *choose_icon(int code, int is_day)
{
    switch (code)
    {
    // Clear sky
    case 0:
        return is_day
                   ? &icon_sunny
                   : &icon_clear_night;

    // Mainly clear
    case 1:
        return is_day
                   ? &icon_mostly_sunny
                   : &icon_mostly_clear_night;

    // Partly cloudy
    case 2:
        return is_day
                   ? &icon_partly_cloudy
                   : &icon_partly_cloudy_night;

    // Overcast
    case 3:
        return &icon_cloudy;

    // Fog / mist
    case 45:
    case 48:
        return &icon_haze_fog_dust_smoke;

    // Drizzle (light → dense)
    case 51:
    case 53:
    case 55:
        return &icon_drizzle;

    // Freezing drizzle
    case 56:
    case 57:
        return &icon_sleet_hail;

    // Rain: slight showers
    case 61:
        return is_day
                   ? &icon_scattered_showers_day
                   : &icon_scattered_showers_night;

    // Rain: moderate
    case 63:
        return &icon_showers_rain;

    // Rain: heavy
    case 65:
        return &icon_heavy_rain;

    // Freezing rain
    case 66:
    case 67:
        return &icon_wintry_mix_rain_snow;

    // Snow fall (light, moderate, heavy) & snow showers (light)
    case 71:
    case 73:
    case 75:
    case 85:
        return &icon_snow_showers_snow;

    // Snow grains
    case 77:
        return &icon_flurries;

    // Rain showers (slight → moderate)
    case 80:
    case 81:
        return is_day
                   ? &icon_scattered_showers_day
                   : &icon_scattered_showers_night;

    // Rain showers: violent
    case 82:
        return &icon_heavy_rain;

    // Heavy snow showers
    case 86:
        return &icon_heavy_snow;

    // Thunderstorm (light)
    case 95:
        return is_day
                   ? &icon_isolated_scattered_tstorms_day
                   : &icon_isolated_scattered_tstorms_night;

    // Thunderstorm with hail
    case 96:
    case 99:
        return &icon_strong_tstorms;

    // Fallback for any other code
    default:
        return is_day
                   ? &icon_mostly_cloudy_day
                   : &icon_mostly_cloudy_night;
    }
}

int day_of_week(int y, int m, int d)
{
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3)
        y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

String hour_of_day(int hour)
{
    const LocalizedStrings *strings = get_strings(LANG_EN);
    if (hour < 0 || hour > 23)
        return String(strings->invalid_hour);

    const bool use_24_hour = false;
    if (use_24_hour)
    {
        if (hour < 10)
            return String("0") + String(hour);
        else
            return String(hour);
    }
    else
    {
        if (hour == 0)
            return String("12") + strings->am;
        if (hour == 12)
            return String(strings->noon);

        bool isMorning = (hour < 12);
        String suffix = isMorning ? strings->am : strings->pm;

        int displayHour = hour % 12;

        return String(displayHour) + suffix;
    }
}

void update_weather(lv_timer_t *timer)
{
    auto strings = get_strings(LANG_EN);

    String url = String("http://api.open-meteo.com/v1/forecast?latitude=") + latitude + "&longitude=" + longitude + "&current=temperature_2m,apparent_temperature,is_day,weather_code" + "&daily=temperature_2m_min,temperature_2m_max,weather_code" + "&hourly=temperature_2m,precipitation_probability,is_day,weather_code" + "&forecast_hours=7" + "&timezone=auto";

    HTTPClient http;
    http.begin(url);

    if (http.GET() == HTTP_CODE_OK)
    {
        Serial.println("Updated weather from open-meteo: " + url);

        String payload = http.getString();
        JsonDocument doc;

        if (deserializeJson(doc, payload) == DeserializationError::Ok)
        {
            float t_now = doc["current"]["temperature_2m"].as<float>();
            float t_ap = doc["current"]["apparent_temperature"].as<float>();
            int code_now = doc["current"]["weather_code"].as<int>();
            int is_day = doc["current"]["is_day"].as<int>();

            const bool use_fahrenheit = true;
            if (use_fahrenheit)
            {
                t_now = t_now * 9.0 / 5.0 + 32.0;
                t_ap = t_ap * 9.0 / 5.0 + 32.0;
            }

            char unit = use_fahrenheit ? 'F' : 'C';
            lv_label_set_text_fmt(objects.current_temperature_label, "%.0f°%c", t_now, unit);
            lv_label_set_text_fmt(objects.feels_temperature_label, "%.0f°%c", t_ap, unit);
            lv_img_set_src(objects.current_conditions_image, choose_image(code_now, is_day));

            Serial.println(String("Forecast display mode is ") + (display_seven_day_forecast ? "7-day" : "hourly"));
            if (display_seven_day_forecast)
            {
                lv_label_set_text(objects.forecast_type_label, strings->seven_day_forecast);

                JsonArray times = doc["daily"]["time"].as<JsonArray>();
                JsonArray tmin = doc["daily"]["temperature_2m_min"].as<JsonArray>();
                JsonArray tmax = doc["daily"]["temperature_2m_max"].as<JsonArray>();
                JsonArray weather_codes = doc["daily"]["weather_code"].as<JsonArray>();

                for (int i = 0; i < 7; i++)
                {
                    const char *date = times[i];
                    int year = atoi(date + 0);
                    int mon = atoi(date + 5);
                    int dayd = atoi(date + 8);
                    int dow = day_of_week(year, mon, dayd);
                    const char *dayStr = (i == 0) ? strings->today : strings->weekdays[dow];

                    float mn = tmin[i].as<float>();
                    float mx = tmax[i].as<float>();
                    if (use_fahrenheit)
                    {
                        mn = mn * 9.0 / 5.0 + 32.0;
                        mx = mx * 9.0 / 5.0 + 32.0;
                    }

                    lv_label_set_text_fmt(forecast_datetime_label[i], "%s", dayStr);
                    lv_label_set_text_fmt(forecast_temp_label[i], "%.0f°%c", mx, unit);
                    lv_label_set_text_fmt(forecast_precip_low_label[i], "%.0f°%c", mn, unit);
                    lv_img_set_src(forecast_visibility_image[i], choose_icon(weather_codes[i].as<int>(), (i == 0) ? is_day : 1));
                }
            }
            else
            {
                lv_label_set_text(objects.forecast_type_label, strings->hourly_forecast);

                JsonArray hours = doc["hourly"]["time"].as<JsonArray>();
                JsonArray hourly_temps = doc["hourly"]["temperature_2m"].as<JsonArray>();
                JsonArray precipitation_probabilities = doc["hourly"]["precipitation_probability"].as<JsonArray>();
                JsonArray hourly_weather_codes = doc["hourly"]["weather_code"].as<JsonArray>();
                JsonArray hourly_is_day = doc["hourly"]["is_day"].as<JsonArray>();

                for (int i = 0; i < 7; i++)
                {
                    const char *date = hours[i]; // "YYYY-MM-DD"
                    int hour = atoi(date + 11);
                    int minute = atoi(date + 14);
                    String hour_name = hour_of_day(hour);

                    float precipitation_probability = precipitation_probabilities[i].as<float>();
                    float temp = hourly_temps[i].as<float>();
                    if (use_fahrenheit)
                    {
                        temp = temp * 9.0 / 5.0 + 32.0;
                    }

                    if (i == 0)
                    {
                        lv_label_set_text(forecast_datetime_label[i], strings->now);
                    }
                    else
                    {
                        lv_label_set_text(forecast_datetime_label[i], hour_name.c_str());
                    }
                    lv_label_set_text_fmt(forecast_temp_label[i], "%.0f°%c", temp, unit);
                    lv_label_set_text_fmt(forecast_precip_low_label[i], "%.0f%%", precipitation_probability);
                    lv_img_set_src(forecast_visibility_image[i], choose_icon(hourly_weather_codes[i].as<int>(), hourly_is_day[i].as<int>()));
                }
            }
        }
        else
        {
            Serial.println("JSON parse failed on result from " + url);
        }
    }
    else
    {
        Serial.println("HTTP GET failed at " + url);
    }
    http.end();
}

void toggle_seven_day_forecast()
{
    display_seven_day_forecast = !display_seven_day_forecast;
    preferences.putBool("display_7day", display_seven_day_forecast);

    Serial.println(String("Toggled forecast display mode to ") + (display_seven_day_forecast ? "7-day" : "hourly"));
    update_weather(nullptr);
}
