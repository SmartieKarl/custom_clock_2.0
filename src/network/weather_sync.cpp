#include "weather_sync.h"
#include "config.h"
#include "network_manager.h"
#include "scheduler.h"

#include <HTTPClient.h>
#include <cstdlib>
#include <cstring>

WeatherSync weatherSync; // Global shared instance

EXT_RAM_ATTR uint8_t WeatherSync::iconBuffer_[MAX_WEATHER_ICON_SIZE]; // Static weather icon buffer
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

// Syncs weather from OpenWeatherMap
bool WeatherSync::sync()
{
    snapshot_.valid = false;
    snapshot_.tempC = 0;
    snapshot_.tempMaxC = 0;
    snapshot_.tempMinC = 0;
    snapshot_.condition[0] = '\0';
    snapshot_.iconCode[0] = '\0';
    snapshot_.updatedAt = 0;

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
    success = parseJsonInt(payload, "temp", snapshot_.tempC) && success;
    success = parseJsonInt(payload, "temp_max", snapshot_.tempMaxC) && success;
    success = parseJsonInt(payload, "temp_min", snapshot_.tempMinC) && success;
    success = parseWeatherCondition(payload, snapshot_.condition, sizeof(snapshot_.condition)) && success;
    success = parseJsonString(payload, "icon", snapshot_.iconCode, sizeof(snapshot_.iconCode)) && success;

    // Fetch the 50x50 PNG directly into PSRAM if JSON parsing succeeded
    if (success)
    {
        success = downloadIcon_(snapshot_.iconCode);
    }

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

// Downloads a 50x50 weather status icon from rodrigokamada's wonderful icon repo
bool WeatherSync::downloadIcon_(const char *iconCode)
{
    if (!iconCode || iconCode[0] == '\0')
        return false;

    char imgUrl[256];
    std::snprintf(
        imgUrl,
        sizeof(imgUrl),
        "https://rodrigokamada.github.io/openweathermap/images/%s_t.png",
        iconCode);

    HTTPClient http;
    http.setTimeout(10000);

    if (!http.begin(imgUrl))
        return false;

    const int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        http.end();
        return false;
    }

    int totalLen = http.getSize();
    
    // Ensure downloaded image fits inside static buffer
    if (totalLen <= 0 || static_cast<size_t>(totalLen) > MAX_WEATHER_ICON_SIZE)
    {
        http.end();
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    size_t bytesRead = 0;

    // Stream PNG bytes into icon buffer
    while (http.connected() && (bytesRead < static_cast<size_t>(totalLen)))
    {
        size_t availableBytes = stream->available();
        if (availableBytes)
        {
            // Read directly into iconBuffer_ offset
            int c = stream->readBytes(iconBuffer_ + bytesRead, availableBytes);
            bytesRead += c;
        }
        vTaskDelay(1);
    }

    snapshot_.imgSize = bytesRead;
    http.end();

    return (bytesRead == static_cast<size_t>(totalLen));
}