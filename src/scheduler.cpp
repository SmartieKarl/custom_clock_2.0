#include "scheduler.h"
#include "timekeeper.h"

Scheduler scheduler; // Global shared instance

// Run the scheduler
void Scheduler::run()
{
    // Run once a minute
    if (!timekeeper.minuteTick())
        return;

    DateTime time = timekeeper.time();

    for (auto &item : schedule_)
    {
        for (int16_t ts : item.triggerTimestamps)
        {
            if (ts < 0)
                continue;

            const uint8_t hour = ts / 100;
            const uint8_t minute = ts % 100;

            const bool hourMatch = hour == 24 || hour == time.hour();
            const bool minuteMatch = minute == 60 || minute == time.minute();

            if (hourMatch && minuteMatch)
            {
                if (item.callback)
                    item.callback(item.context);
                break;
            }
        }
    }
}

// Sets the callback function of an existing scheduledItem.
bool Scheduler::registerCallback(const char *name, void (*cb)(void *), void *context)
{
    int8_t index = findScheduleIndexByName(name);
    if (index < 0)
        return false; // Name not found in schedule_

    schedule_[index].callback = cb;
    return true;
}

// HOW TO USE:
//  First arg is the name of the scheduled item you are going to modify.
//  Second arg is a timestamp representing a 24-hour format time. No leading zeroes.
//  Ex: 0, 432, 1615, 2359. Single-time values go from 0-2359.
//  Putting the hour at 24 will make the item run every hour on the minute.
//  Putting the minute at 60 will make the item run every minute on the hour.
bool Scheduler::setTimestamps(const char *name, std::initializer_list<int16_t> timestamps)
{
    if (timestamps.size() > MAX_TIMESTAMPS)
        return false;

    const int8_t itemIndex = findScheduleIndexByName(name);
    if (itemIndex < 0)
        return false;

    auto &item = schedule_[itemIndex];
    auto tsIt = timestamps.begin();
    for (int i = 0; i < MAX_TIMESTAMPS; i++)
    {
        if (tsIt != timestamps.end())
            item.triggerTimestamps[i] = *tsIt++;
        else
            item.triggerTimestamps[i] = -1; // Replace with invalid timestamp
    }
    return true;
}

int8_t Scheduler::findScheduleIndexByName(const char *name) const
{
    bool found = false;
    int8_t index = 0;
    for (const auto &item : schedule_)
    {
        if (strcmp(item.name, name) == 0)
            return index;
        index++;
    }
    return -1;
}