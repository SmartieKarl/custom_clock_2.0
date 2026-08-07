#include "timekeeper.h"
#include "config.h"

Timekeeper timekeeper; // Global shared instance

// Constructor
Timekeeper::Timekeeper()
    : rtc_(nullptr),
      currentTime_(DateTime(2000, 1, 1, 0, 0, 0)),
      previousTime_(DateTime(2000, 1, 1, 0, 0, 0)),
      tick_(false), minuteTick_(false), hourTick_(false), dayTick_(false),
      mutex_(NULL), taskHandle_(NULL)
{
}

// Initialize semaphores and prepare sqw pin interrupt
void Timekeeper::begin(RTC_DS3231 &rtc)
{
    rtc_ = &rtc;
    mutex_ = xSemaphoreCreateMutex();

    rtc_->writeSqwPinMode(DS3231_SquareWave1Hz);

    currentTime_ = rtc_->now();
    previousTime_ = currentTime_;

    pinMode(Pins::DS3231_SQW, INPUT_PULLUP);

    attachInterruptArg(digitalPinToInterrupt(Pins::DS3231_SQW), sqwISR, this, FALLING);

    xTaskCreatePinnedToCore(
        taskRunner,
        "TimekeeperTask",
        2048,
        this,
        2,
        &taskHandle_,
        1);
}

// Syncs software time with hardware time and caches old time
void Timekeeper::update()
{
    xSemaphoreTake(mutex_, portMAX_DELAY);
    previousTime_ = currentTime_;
    currentTime_ = rtc_->now();

    if (currentTime_ != previousTime_)
        tick_ = true;

    if (currentTime_.minute() != previousTime_.minute())
        minuteTick_ = true;

    if (currentTime_.hour() != previousTime_.hour())
        hourTick_ = true;

    if (currentTime_.day() != previousTime_.day())
        dayTick_ = true;
    xSemaphoreGive(mutex_);
}

// Notifies the taskRunner to update the timekeeper. Activates when DS3231's SQW pin is pulled low.
void IRAM_ATTR Timekeeper::sqwISR(void *arg)
{
    Timekeeper *instance = static_cast<Timekeeper *>(arg);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (instance->taskHandle_ != NULL)
        vTaskNotifyGiveFromISR(instance->taskHandle_, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void Timekeeper::taskRunner(void *pvParameters)
{
    // Set timekeeper instance to this
    Timekeeper *instance = static_cast<Timekeeper *>(pvParameters);

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        instance->update();
    }
}

// Returns cached time
DateTime Timekeeper::time() const
{
    xSemaphoreTake(mutex_, portMAX_DELAY);
    DateTime t = currentTime_;
    xSemaphoreGive(mutex_);
    return t;
}

// Sets the time. Only a simple access layer! Does not check input validity.
void Timekeeper::setTime(DateTime time)
{
    rtc_->adjust(time);
}

// Returns whether time has changed
bool Timekeeper::tick()
{
    xSemaphoreTake(mutex_, portMAX_DELAY);
    bool r = tick_;
    if (tick_)
        tick_ = false;
    xSemaphoreGive(mutex_);
    return r;
}

// Returns whether minute has changed
bool Timekeeper::minuteTick()
{
    xSemaphoreTake(mutex_, portMAX_DELAY);
    bool r = minuteTick_;
    if (minuteTick_)
        minuteTick_ = false;
    xSemaphoreGive(mutex_);
    return r;
}

// Returns whether hour has changed
bool Timekeeper::hourTick()
{
    xSemaphoreTake(mutex_, portMAX_DELAY);
    bool r = hourTick_;
    if (hourTick_)
        hourTick_ = false;
    xSemaphoreGive(mutex_);
    return r;
}

// Returns whether day has changed
bool Timekeeper::dayTick()
{
    xSemaphoreTake(mutex_, portMAX_DELAY);
    bool r = dayTick_;
    if (dayTick_)
        dayTick_ = false;
    xSemaphoreGive(mutex_);
    return r;
}