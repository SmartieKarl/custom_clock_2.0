#include "weather_sync.h"
#include "config.h"
#include "network_manager.h"
#include "scheduler.h"

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
        4096,
        this,
        1,
        &taskHandle_,
        0);
}

// Syncs weather from OpenWeatherMap
bool WeatherSync::sync()
{
    snapshot_.valid = false;
    snapshot_.tempC = 0;
    snapshot_.tempMaxC = 0;
    snapshot_.tempMinC = 0;
    snapshot_.humidity = 0;
    snapshot_.condition[0] = '\0';
    snapshot_.updatedAt = 0;

    if (!networkManager.startWiFiSession())
        return false;

    char url[256];
    std::snprintf(
        url,
        sizeof(url),
        "http://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=%s",
        Secret::WEATHER_LOCATION,
        Secret::WEATHER_API_KEY,
        Secret::WEATHER_UNITS);

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

    bool success = false;
    success = parseJsonInt(payload, "temp", snapshot_.tempC);
    success = parseJsonInt(payload, "temp_max", snapshot_.tempMaxC) && success;
    success = parseJsonInt(payload, "temp_min", snapshot_.tempMinC) && success;
    success = parseJsonInt(payload, "humidity", snapshot_.humidity) && success;
    success = parseWeatherCondition(payload, snapshot_.condition, sizeof(snapshot_.condition)) && success;

    if (success)
    {
        snapshot_.valid = true;
        snapshot_.updatedAt = static_cast<uint32_t>(millis() / 1000);
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
        instance->sync();
        // LOG IMPLEMENTATION HERE (if not sync)
    }
}