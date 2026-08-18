#pragma once

#include <Arduino.h>
#include <RTClib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// timekeeper.h
// Central mutex protected time access module.
// Starts its own task to monitor and update time universally.

class Timekeeper
{
  public:
    Timekeeper();

    void begin(RTC_DS3231 &rtc);

    DateTime time() const;
    void setTime(DateTime time);

    bool tick();
    bool minuteTick();
    bool hourTick();
    bool dayTick();

    void clearTickFlags();

  private:
    RTC_DS3231 *rtc_;

    DateTime currentTime_;
    DateTime previousTime_;

    bool tick_;
    bool minuteTick_;
    bool hourTick_;
    bool dayTick_;

    mutable SemaphoreHandle_t mutex_;

    TaskHandle_t taskHandle_;

    void update();

    static void IRAM_ATTR sqwISR(void *arg);
    static void taskRunner(void *pvParameters);
};

extern Timekeeper timekeeper; // Universal Timekeeper