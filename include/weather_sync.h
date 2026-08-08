#pragma once

#include <Arduino.h>

// weather_sync.h
// Hosts a FreeRTOS task that gets weather info from OpenWeatherMap API.

class WeatherSync
{
  public:
    struct WeatherSnapshot
    {
        bool valid = false;
        int tempC = 0;
        int tempMaxC = 0;
        int tempMinC = 0;
        int humidity = 0;
        char condition[16] = "";
        uint32_t updatedAt = 0;
    };

    WeatherSync();

    bool begin();
    bool sync();
    const WeatherSnapshot &snapshot() const { return snapshot_; }

    void syncViaTaskRunner();

  private:
    WeatherSnapshot snapshot_;

    TaskHandle_t taskHandle_;
    static void taskRunner_(void *pvParameters);
};

extern WeatherSync weatherSync; // Universal WeatherSync