#include <lvgl.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include "text_strings.h"
#include "forecast_weather.h"
#include "forecast_widgets.h"
#include "forecast_preferences.h"
#include "forecast_mqtt.h"
#include "ui/ui.h"

float temperature_now = 0.0;
float feels_like_temperature = 0.0;

const char *chooseImage(int code, int is_day)
{
    switch (code)
    {
    // Clear sky
    case 0:
        return is_day
                   ? "S:/images/image_sunny.bin"
                   : "S:/images/image_clear_night.bin";

    // Mainly clear
    case 1:
        return is_day
                   ? "S:/images/image_mostly_sunny.bin"
                   : "S:/images/image_mostly_clear_night.bin";

    // Partly cloudy
    case 2:
        return is_day
                   ? "S:/images/image_partly_cloudy.bin"
                   : "S:/images/image_partly_cloudy_night.bin";

    // Overcast
    case 3:
        return "S:/images/image_cloudy.bin";

    // Fog / mist
    case 45:
    case 48:
        return "S:/images/image_haze_fog.bin";

    // Drizzle (light → dense)
    case 51:
    case 53:
    case 55:
        return "S:/images/image_drizzle.bin";

    // Freezing drizzle
    case 56:
    case 57:
        return "S:/images/image_sleet_hail.bin";

    // Rain: slight showers
    case 61:
        return is_day
                   ? "S:/images/image_scat_shwrs_day.bin"
                   : "S:/images/image_scat_shwrs_night.bin";

    // Rain: moderate
    case 63:
        return "S:/images/image_showers_rain.bin";

    // Rain: heavy
    case 65:
        return "S:/images/image_heavy_rain.bin";

    // Freezing rain
    case 66:
    case 67:
        return "S:/images/image_wintry_mix.bin";

    // Snow fall (light, moderate, heavy) & snow showers (light)
    case 71:
    case 73:
    case 75:
    case 85:
        return "S:/images/image_snow_showers_snow.bin";

    // Snow grains
    case 77:
        return "S:/images/image_flurries.bin";

    // Rain showers (slight → moderate)
    case 80:
    case 81:
        return is_day
                   ? "S:/images/image_scat_shwrs_day.bin"
                   : "S:/images/image_scat_shwrs_night.bin";

    // Rain showers: violent
    case 82:
        return "S:/images/image_heavy_rain.bin";

    // Heavy snow showers
    case 86:
        return "S:/images/image_heavy_snow.bin";

    // Thunderstorm (light)
    case 95:
        return is_day
                   ? "S:/images/image_iso_scat_ts_day.bin"
                   : "S:/images/image_iso_scat_ts_night.bin";

    // Thunderstorm with hail
    case 96:
    case 99:
        return "S:/images/image_strong_tstorms.bin";

    // Fallback for any other code
    default:
        return is_day
                   ? "S:/images/image_mostly_cloudy_day.bin"
                   : "S:/images/image_mostly_cloudy_night.bin";
    }
}

const char *chooseIcon(int code, int is_day)
{
    switch (code)
    {
    // Clear sky
    case 0:
        return is_day
                   ? "S:/images/icon_sunny.bin"
                   : "S:/images/icon_clear_night.bin";

    // Mainly clear
    case 1:
        return is_day
                   ? "S:/images/icon_mostly_sunny.bin"
                   : "S:/images/icon_mostly_clear_night.bin";

    // Partly cloudy
    case 2:
        return is_day
                   ? "S:/images/icon_partly_cloudy.bin"
                   : "S:/images/icon_partly_cloudy_night.bin";

    // Overcast
    case 3:
        return "S:/images/icon_cloudy.bin";

    // Fog / mist
    case 45:
    case 48:
        return "S:/images/icon_haze_fog.bin";

    // Drizzle (light → dense)
    case 51:
    case 53:
    case 55:
        return "S:/images/icon_drizzle.bin";

    // Freezing drizzle
    case 56:
    case 57:
        return "S:/images/icon_sleet_hail.bin";

    // Rain: slight showers
    case 61:
        return is_day
                   ? "S:/images/icon_scat_shwrs_day.bin"
                   : "S:/images/icon_scat_shwrs_night.bin";

    // Rain: moderate
    case 63:
        return "S:/images/icon_showers_rain.bin";

    // Rain: heavy
    case 65:
        return "S:/images/icon_heavy_rain.bin";

    // Freezing rain
    case 66:
    case 67:
        return "S:/images/icon_wintry_mix.bin";

    // Snow fall (light, moderate, heavy) & snow showers (light)
    case 71:
    case 73:
    case 75:
    case 85:
        return "S:/images/icon_snow_showers_snow.bin";

    // Snow grains
    case 77:
        return "S:/images/icon_flurries.bin";

    // Rain showers (slight → moderate)
    case 80:
    case 81:
        return is_day
                   ? "S:/images/icon_scat_shwrs_day.bin"
                   : "S:/images/icon_scat_shwrs_night.bin";

    // Rain showers: violent
    case 82:
        return "S:/images/icon_heavy_rain.bin";

    // Heavy snow showers
    case 86:
        return "S:/images/icon_heavy_snow.bin";

    // Thunderstorm (light)
    case 95:
        return is_day
                   ? "S:/images/icon_iso_scat_ts_day.bin"
                   : "S:/images/icon_iso_scat_ts_night.bin";

    // Thunderstorm with hail
    case 96:
    case 99:
        return "S:/images/icon_strong_tstorms.bin";

    // Fallback for any other code
    default:
        return is_day
                   ? "S:/images/icon_mostly_cloudy_day.bin"
                   : "S:/images/icon_mostly_cloudy_night.bin";
    }
}

int dayOfWeek(int y, int m, int d)
{
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3)
        y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

String hourOfDay(int hour)
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

void updateWeather(lv_timer_t *timer)
{
    auto strings = get_strings(LANG_EN);
    auto latitude = String(weather_latitude);
    auto longitude = String(weather_longitude);

    String url = String("http://api.open-meteo.com/v1/forecast?latitude=") + latitude + "&longitude=" + longitude + "&current=temperature_2m,apparent_temperature,is_day,weather_code" + "&daily=temperature_2m_min,temperature_2m_max,weather_code" + "&hourly=temperature_2m,precipitation_probability,is_day,weather_code" + "&forecast_hours=7" + "&timezone=auto";

    HTTPClient http;
    http.begin(url);

    Log.infoln("Fetching weather data for lat=%s, lon=%s", latitude.c_str(), longitude.c_str());
    auto status = http.GET();
    if (status == HTTP_CODE_OK)
    {
        Log.infoln("Updated weather from open-meteo: %s", url.c_str());

        String payload = http.getString();
        JsonDocument doc;

        if (deserializeJson(doc, payload) == DeserializationError::Ok)
        {
            float t_now = doc["current"]["temperature_2m"].as<float>();
            float t_ap = doc["current"]["apparent_temperature"].as<float>();
            int code_now = doc["current"]["weather_code"].as<int>();
            int is_day = doc["current"]["is_day"].as<int>();

            temperature_now = t_now;
            feels_like_temperature = t_ap;
            
            if (use_fahrenheit)
            {
                t_now = t_now * 9.0 / 5.0 + 32.0;
                t_ap = t_ap * 9.0 / 5.0 + 32.0;
            }

            char unit = use_fahrenheit ? 'F' : 'C';
            lv_label_set_text_fmt(objects.current_temperature_label, "%.0f°%c", t_now, unit);
            lv_label_set_text_fmt(objects.feels_temperature_label, "%.0f°%c", t_ap, unit);
            lv_img_set_src(objects.current_conditions_image, chooseImage(code_now, is_day));

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
                    int dow = dayOfWeek(year, mon, dayd);
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
                    lv_img_set_src(forecast_visibility_image[i], chooseIcon(weather_codes[i].as<int>(), (i == 0) ? is_day : 1));
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
                    String hour_name = hourOfDay(hour);

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
                    lv_img_set_src(forecast_visibility_image[i], chooseIcon(hourly_weather_codes[i].as<int>(), hourly_is_day[i].as<int>()));
                }
            }
        }
        else
        {
            Log.errorln("JSON parse failed on result from %s", url.c_str());
        }
    }
    else
    {
        Log.errorln("HTTP GET failed at %s with status %d", url.substring(0, 50).c_str(), status);
    }
    http.end();

    publishSensorState();
}

void toggleSevenDayForecast()
{
    display_seven_day_forecast = !display_seven_day_forecast;
    preferences.putBool("display_7day", display_seven_day_forecast);

    updateWeather(nullptr);
}
