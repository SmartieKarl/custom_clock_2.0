#include "time_sync.h"
#include "network_manager.h"
#include "scheduler.h"

TimeSync timeSync; // Global shared instance

TimeSync::TimeSync()
    : taskHandle_(NULL)
{
}

bool TimeSync::begin(RTC_DS3231 &rtc)
{
    rtc_ = &rtc;

    if (!scheduler.registerCallback(
            "timeSync",
            [](void *context)
            {
                static_cast<TimeSync *>(context)->syncViaTaskRunner();
            },
            this))
        return false;

    // Initialize the TimeSync task
    xTaskCreatePinnedToCore(
        taskRunner_,
        "TimeSyncTask",
        4096,
        this,
        1,
        &taskHandle_,
        0);

    return taskHandle_ != NULL;
}

// Syncs time via NTP servers
bool TimeSync::sync()
{
    if (!networkManager.startWiFiSession())
        return false;

    struct tm timeinfo = {};
    unsigned long start = millis();
    const unsigned long ntpTimeout = 10000;

    bool gotTime = false;
    while (millis() - start < ntpTimeout)
    {
        if (getLocalTime(&timeinfo))
        {
            gotTime = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (!gotTime)
    {
        networkManager.endWiFiSession();
        return false;
    }

    DateTime dt(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec);

    rtc_->adjust(dt);

    networkManager.endWiFiSession();
    return true;
}

// Notifies the taskRunner to run sync(). Recommended to prevent blocking.
void TimeSync::syncViaTaskRunner()
{
    if (taskHandle_ != NULL)
        xTaskNotifyGive(taskHandle_);
}

void TimeSync::taskRunner_(void *pvParameters)
{
    // Set TimeSync instance to this
    TimeSync *instance = static_cast<TimeSync *>(pvParameters);

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        instance->sync();
        // LOG IMPLEMENTATION HERE (if not sync)
    }
}