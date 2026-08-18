#include "weather_sync.h"
#include "config.h"
#include "network_manager.h"
#include "scheduler.h"
#include "ui.h"

#include <HTTPClient.h>
#include <cstdlib>
#include <cstring>

WeatherSync weatherSync; // Global shared instance

namespace
{
bool parseJsonInt(const char *json, const char *key, int &value)
{
    char needle[32];
    std::snprintf(needle, sizeof(needle), "\"%s\":", key);

    const char *p = std::strstr(json, needle);
    if (!p)
        return false;

    p += std::strlen(needle);
    char *end = nullptr;
    const double parsed = std::strtod(p, &end);
    if (p == end)
        return false;

    value = static_cast<int>(parsed);
    return true;
}

bool parseWeatherCondition(const char *json, char *out, size_t outSize)
{
    const char *weatherSection = std::strstr(json, "\"weather\"");
    if (!weatherSection)
        return false;

    const char *mainKey = std::strstr(weatherSection, "\"main\":\"");
    if (!mainKey)
        return false;

    mainKey += std::strlen("\"main\":\"");
    const char *mainEnd = std::strchr(mainKey, '"');
    if (!mainEnd)
        return false;

    const size_t length = static_cast<size_t>(mainEnd - mainKey);
    if (length >= outSize)
        return false;

    std::memcpy(out, mainKey, length);
    out[length] = '\0';
    return true;
}

bool parseJsonString(const char *json, const char *key, char *out, size_t outSize)
{
    char needle[32];
    std::snprintf(needle, sizeof(needle), "\"%s\":\"", key);

    const char *p = std::strstr(json, needle);
    if (!p)
        return false;

    p += std::strlen(needle);
    const char *end = std::strchr(p, '"');
    if (!end)
        return false;

    const size_t length = static_cast<size_t>(end - p);
    if (length >= outSize)
        return false;

    std::memcpy(out, p, length);
    out[length] = '\0';
    return true;
}
} // namespace

WeatherSync::WeatherSync()
    : snapshot_{}, taskHandle_(NULL)
{
}

bool WeatherSync::begin()
{
    if (!scheduler.registerCallback(
            "weatherSync",
            [](void *context)
            {
                static_cast<WeatherSync *>(context)->syncViaTaskRunner();
            },
            this))
        return false;

    // Initialize the WeatherSync task
    xTaskCreatePinnedToCore(
        taskRunner_,
        "WeatherSyncTask",
        8192,
        this,
        1,
        &taskHandle_,
        0);

    return taskHandle_ != NULL;
}

// Light wrapper that both calls sync_() and updates the weather panel
bool WeatherSync::sync()
{
    bool success = sync_();
    if (success)
        ui_update_weather_panel(snapshot_.valid, snapshot_.iconCode, snapshot_.tempF);

    return success;
}

// Syncs weather from OpenWeatherMap
bool WeatherSync::sync_()
{
    snapshot_.valid = false;

    if (!networkManager.startWiFiSession())
        return false;

    char url[256];
    std::snprintf(
        url,
        sizeof(url),
        "http://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=%s",
        WEATHER_LOCATION,
        WEATHER_API_KEY,
        WEATHER_UNITS);

    HTTPClient http;
    http.setTimeout(10000);

    if (!http.begin(url))
    {
        networkManager.endWiFiSession();
        return false;
    }

    const int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        http.end();
        networkManager.endWiFiSession();
        return false;
    }

    char payload[2048] = {0};
    WiFiClient *stream = http.getStreamPtr();
    size_t len = 0;

    while (stream->available() && len < sizeof(payload) - 1)
    {
        const int ch = stream->read();
        if (ch < 0)
            break;
        payload[len++] = static_cast<char>(ch);
    }
    payload[len] = '\0';

    http.end();

    // Parse Weather Metrics
    bool success = true;
    success = parseJsonInt(payload, "temp", snapshot_.tempF) && success;
    success = parseJsonInt(payload, "temp_max", snapshot_.tempMaxF) && success;
    success = parseJsonInt(payload, "temp_min", snapshot_.tempMinF) && success;
    success = parseWeatherCondition(payload, snapshot_.condition, sizeof(snapshot_.condition)) && success;
    success = parseJsonString(payload, "icon", snapshot_.iconCode, sizeof(snapshot_.iconCode)) && success;

    if (success)
    {
        snapshot_.valid = true;
        snapshot_.lastValidUpdateAt = static_cast<uint32_t>(millis() / 1000);
    }

    networkManager.endWiFiSession();
    return success;
}

// Notifies the taskRunner to run sync(). Recommended to prevent blocking.
void WeatherSync::syncViaTaskRunner()
{
    if (taskHandle_ != NULL)
        xTaskNotifyGive(taskHandle_);
}

void WeatherSync::taskRunner_(void *pvParameters)
{
    // Set WeatherSync instance to this
    WeatherSync *instance = static_cast<WeatherSync *>(pvParameters);

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (instance->sync_())
            ui_update_weather_panel(instance->snapshot_.valid, instance->snapshot_.iconCode, instance->snapshot_.tempF);
        // LOG IMPLEMENTATION HERE (if not sync)
    }
}