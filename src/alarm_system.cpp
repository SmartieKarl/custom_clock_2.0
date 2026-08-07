#include "alarm_system.h"
#include "config.h"
#include <Wire.h>

#include "audio_control.h"
#include "rfid_control.h"
#include "scheduler.h"
#include "timekeeper.h"

AlarmSystem alarmSystem; // Global shared instance

// Constructor
AlarmSystem::AlarmSystem()
    : ringing_(false), alarm_({0, 00, false}), taskHandle_(NULL)
{
}

// Initialize the alarm system
bool AlarmSystem::begin(RTC_DS3231 &rtc)
{
    rtc_ = &rtc;
    // Read and set alarm state
    DateTime rtcAlarm = rtc_->getAlarm1();
    alarm_.hour = rtcAlarm.hour();
    alarm_.minute = rtcAlarm.minute();
    alarm_.enabled = isRTCAlarmEnabled_();

    if (!scheduler.registerCallback(
            "alarm",
            [](void *context)
            {
                static_cast<AlarmSystem *>(context)->triggerAlarm();
            },
            this))
        return false;

    if (alarm_.enabled)
    {
        scheduler.setTimestamps("alarm", {alarm_.timestamp()});

        // Power loss safety to re-trigger alarm
        if (rtc_->alarmFired(RTC_ALARM_NUM_))
            triggerAlarm();
    }
    else
    {
        scheduler.setTimestamps("alarm", {});
        rtc_->clearAlarm(RTC_ALARM_NUM_);
    }

    // Initialize the alarm task
    xTaskCreatePinnedToCore(
        taskRunner,
        "AlarmTask",
        2048,
        this,
        2,
        &taskHandle_,
        1);

    return true;
}

// Sets off the alarm and notifies the alarm task to run
void AlarmSystem::triggerAlarm()
{
    // Play alarm track
    audioControl.setVolume(alarmVolume_);
    audioControl.connectToSD(alarmTrackFileName_);

    ringing_ = true;

    if (taskHandle_ != NULL)
        xTaskNotifyGive(taskHandle_);
}

// Stops the alarm and clears flags
void AlarmSystem::dismissAlarm()
{
    audioControl.stop();
    audioControl.setVolume(AUDIO_DEFAULT_VOLUME);

    rtc_->clearAlarm(RTC_ALARM_NUM_);
    ringing_ = false;
}

void AlarmSystem::taskRunner(void *pvParameters)
{
    // Set AlarmSystem instance to this
    AlarmSystem *instance = static_cast<AlarmSystem *>(pvParameters);

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        uint16_t lastRfidCheck = 0;
        const uint16_t rfidPollInterval = 100; // ms
        char cardUID[64];

        while (instance->ringing_)
        {
            if (millis() - lastRfidCheck >= rfidPollInterval)
            {
                lastRfidCheck - millis();
                rfidControl.poll(cardUID);
                if (!(cardUID[0] = '\0'))
                {
                    instance->dismissAlarm();
                    break;
                }
            }
        }
    }
}

// Returns current alarm
AlarmTime AlarmSystem::getAlarm()
{
    return alarm_;
}

// Sets the alarm
void AlarmSystem::setAlarm(uint8_t hr, uint8_t min, bool enable)
{
    alarm_ = {hr, min, enable};

    rtc_->clearAlarm(RTC_ALARM_NUM_);
    if (enable)
    {
        rtc_->setAlarm1(DateTime(0, 0, 0, hr, min, 0), DS3231_A1_Hour);
        scheduler.setTimestamps("alarm", {alarm_.timestamp()});
    }
    else
    {
        rtc_->disableAlarm(RTC_ALARM_NUM_);
        scheduler.setTimestamps("alarm", {});
    }
}

// Returns if the alarm is ringing
bool AlarmSystem::ringing()
{
    return ringing_;
}

// Read DS3231 alarm register to determine whether hardware alarm is enabled
bool AlarmSystem::isRTCAlarmEnabled_(uint8_t alarmNumber)
{
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_CONTROL);
    Wire.endTransmission(false);
    Wire.requestFrom(DS3231_ADDRESS, 1);
    uint8_t ctrl = Wire.available() ? Wire.read() : 0;
    if (alarmNumber == 1)
        return ctrl & 0x01; // Bit 0: Alarm 1 enabled
    else if (alarmNumber == 2)
        return ctrl & 0x02; // Bit 0: Alarm 2 enabled
    else
        return false;
}