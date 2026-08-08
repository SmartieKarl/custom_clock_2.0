#pragma once

// Shhhh
namespace Secret
{
// WiFi and Clock sync settings
constexpr const char *WIFI_SSID = "CougarsOverUtes";
constexpr const char *WIFI_PASSWORD = "HannigRussell0611";
constexpr const char *TIME_ZONE = "MST7MDT,M3.2.0/2,M11.1.0/2"; // in POSIX time zone format

// Weather API settings (API key from OpenWeatherMap)
constexpr const char *WEATHER_API_KEY = "7493c1fb4f20c8e2262350d3047a4070";
constexpr const char *WEATHER_LOCATION = "Eagle%20Mountain,UT,US";
constexpr const char *WEATHER_UNITS = "imperial";

// Master NFC tag UID
constexpr const char *ALARM_CARD_UID = "047742AD6B6D70";

// Blynk inegration
constexpr const char *BLYNK_TEMPLATE_ID = "TMPL26HlUTVsE";
constexpr const char *BLYNK_TEMPLATE_NAME = "Custom Clock";
constexpr const char *BLYNK_AUTH = "-dzT6qHzOQTMDGbdcka2qJwm-Z3Dwx32";
}; // namespace Secret