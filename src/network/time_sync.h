#pragma once

#include <Arduino.h>
#include <RTClib.h>

// time_sync.h
// Hosts a FreeRTOS task that syncs NTP time with the DS3231's internal time.

class TimeSync
{
  public:
    TimeSync();

    bool begin(RTC_DS3231 &rtc);
    bool sync();
    void syncViaTaskRunner();

  private:
    RTC_DS3231 *rtc_;

    TaskHandle_t taskHandle_;

    static void taskRunner_(void *pvParameters);
};

extern TimeSync timeSync; // Universal TimeSync