#pragma once

#include "config.h"
#include "timekeeper.h"
#include <RTClib.h>
#include <stdint.h>

// alarm_system.h
// Central alarm control system

#define DEFAULT_ALARM_TRACK_FILE_PATH "/test.mp3"
struct AlarmTime
{
    uint8_t hour;
    uint8_t minute;
    bool enabled;
    int16_t timestamp() { return (hour * 100) + minute; }
};

class AlarmSystem
{
  public:
    AlarmSystem();

    bool begin(RTC_DS3231 &rtc);

    void triggerAlarm();
    void dismissAlarm();

    AlarmTime getAlarm();
    void setAlarm(uint8_t hr, uint8_t min, bool enable);

    uint8_t getAlarmVolume() const;
    void setAlarmVolume(uint8_t volume);

    const char *getAlarmTrackFilePath() const;
    bool setAlarmTrackFilePath(const char *path);

    bool ringing();

  private:
    static constexpr uint8_t RTC_ALARM_NUM_ = 1; // RTC has 2 hardware alarms, use only one
    RTC_DS3231 *rtc_;

    bool ringing_;
    AlarmTime alarm_;

    uint8_t alarmVolume_ = 15;
    char alarmTrackFilePath_[32] = DEFAULT_ALARM_TRACK_FILE_PATH;

    TaskHandle_t taskHandle_;

    static void taskRunner_(void *pvParameters);
    bool isRTCAlarmEnabled_(uint8_t alarmNumber = RTC_ALARM_NUM_);
    bool isValidAlarmTrackPath_(const char *path) const;
};

extern AlarmSystem alarmSystem; // Universal AlarmSystem