#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

class WeatherSync
{
  public:
    struct WeatherSnapshot
    {
        bool valid = false;
        int tempF = 0;
        int tempMaxF = 0;
        int tempMinF = 0;
        char condition[16] = "";
        char iconCode[8] = "";

        uint32_t lastValidUpdateAt = 0;
    };

    WeatherSync();

    bool begin();
    bool sync();
    const WeatherSnapshot &snapshot() const { return snapshot_; }

    void syncViaTaskRunner();

  private:
    WeatherSnapshot snapshot_;

    bool sync_();

    TaskHandle_t taskHandle_;
    static void taskRunner_(void *pvParameters);

    bool downloadIcon_(const char *iconCode);
};

extern WeatherSync weatherSync;