#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

// Maximum expected file size for a 50x50 PNG compressed asset (~8 KB)
constexpr size_t MAX_WEATHER_ICON_SIZE = 8192;
class WeatherSync
{
  public:
    struct WeatherSnapshot
    {
        bool valid = false;
        int tempC = 0;
        int tempMaxC = 0;
        int tempMinC = 0;
        char condition[16] = "";
        char iconCode[8] = "";

        size_t imgSize = 0;

        uint32_t updatedAt = 0;
    };

    WeatherSync();

    bool begin();
    bool sync();
    const WeatherSnapshot &snapshot() const { return snapshot_; }

    void syncViaTaskRunner();

  private:
    WeatherSnapshot snapshot_;

    static EXT_RAM_ATTR uint8_t iconBuffer_[MAX_WEATHER_ICON_SIZE];

    TaskHandle_t taskHandle_;
    static void taskRunner_(void *pvParameters);

    bool downloadIcon_(const char *iconCode);
};

extern WeatherSync weatherSync;