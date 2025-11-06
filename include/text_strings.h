#pragma once

// Language support
enum Language { LANG_EN = 0 };

struct LocalizedStrings {
  const char* temp_placeholder;
  const char* feels_like_temp;
  const char* seven_day_forecast;
  const char* hourly_forecast;
  const char* today;
  const char* now;
  const char* am;
  const char* pm;
  const char* noon;
  const char* invalid_hour;
  const char* brightness;
  const char* location;
  const char* use_fahrenheit;
  const char* use_24hr;
  const char* save;
  const char* cancel;
  const char* close;
  const char* location_btn;
  const char* reset_wifi;
  const char* reset;
  const char* change_location;
  const char* aura_settings;
  const char* city;
  const char* search_results;
  const char* city_placeholder;
  const char* wifi_config;
  const char* reset_confirmation;
  const char* language_label;
  const char* weekdays[7];
  const char* use_night_mode;
};

#define DEFAULT_CAPTIVE_SSID "Aura"

static const LocalizedStrings strings_en = {
  "--°C", "Feels Like", "SEVEN DAY FORECAST", "HOURLY FORECAST",
  "Today", "Now", "am", "pm", "Noon", "Invalid hour",
  "Brightness:", "Location:", "Use °F:", "24hr:",
  "Save", "Cancel", "Close", "Location", "Reset Wi-Fi",
  "Reset", "Change Location", "Aura Settings",
  "City:", "Search Results", "e.g. London",
  "Wi-Fi Configuration:\n\n"
  "Please connect your\n"
  "phone or laptop to the\n"
  "temporary Wi-Fi access\n point "
  DEFAULT_CAPTIVE_SSID
  "\n"
  "to configure.\n\n"
  "If you don't see a \n"
  "configuration screen \n"
  "after connecting,\n"
  "visit http://192.168.4.1\n"
  "in your web browser.",
  "Are you sure you want to reset "
  "Wi-Fi credentials?\n\n"
  "You'll need to reconnect to the Wifi SSID " DEFAULT_CAPTIVE_SSID
  " with your phone or browser to "
  "reconfigure Wi-Fi credentials.",
  "Language:",
  {"Sun", "Mon", "Tues", "Wed", "Thurs", "Fri", "Sat"},
  "Dim screen at night"
};


static const LocalizedStrings* get_strings(Language current_language) {
  switch (current_language) {
    default: return &strings_en;
  }
}
