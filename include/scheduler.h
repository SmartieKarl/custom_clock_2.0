#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdint.h>

// scheduler.h
// runs callback functions on a schedule

class Scheduler
{
  public:
    static constexpr uint8_t MAX_TIMESTAMPS = 4;
    static constexpr uint8_t SCHEDULE_SIZE = 3;

    struct ScheduledItem
    {
        const char name[32];
        int16_t triggerTimestamps[MAX_TIMESTAMPS];
        void (*callback)(void *);
        void *context;
    };

    Scheduler() = default;

    void run();

    bool registerCallback(const char *name, void (*cb)(void *), void *context = nullptr);

    bool setTimestamps(const char *name, std::initializer_list<int16_t> timestamps);

  private:
    ScheduledItem schedule_[SCHEDULE_SIZE] =
        {
            {"alarm", {-1, -1, -1, -1}, nullptr, nullptr},
            {"timeSync", {0, -1, -1, -1}, nullptr, nullptr},             // Midnight
            {"blynkConnect", {2400, 2415, 2430, 2445}, nullptr, nullptr} // Every 15 mins
    };

    // Helpers
    int8_t findScheduleIndexByName(const char *name) const;
};

extern Scheduler scheduler; // Univiversal Scheduler
