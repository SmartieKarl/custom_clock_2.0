#pragma once

#include "Audio.h"
#include <Arduino.h>
#include <stdint.h>

// audio_control.h
// Task control layer to enable use of the ESP32_audioI2S library as a seperate, free-running task.

constexpr uint8_t AUDIO_DEFAULT_VOLUME = 15; // 0-21

class AudioTaskControl
{
  public:
    struct audioMessage
    {
        uint8_t cmd;
        const char *txt;
        uint32_t value;
        uint32_t ret;
    };

    AudioTaskControl();
    void begin(uint8_t core = 0, UBaseType_t priority = 2);

    void setVolume(uint8_t vol);
    uint8_t getVolume();
    bool connectToHost(const char *host);
    bool connectToSD(const char *filename);
    bool pause();
    uint32_t stop();

  private:
    void createQueues();
    void taskLoop();
    static void taskTrampoline(void *arg);
    audioMessage transmitReceive(audioMessage msg);

    QueueHandle_t audioSetQueue_;
    QueueHandle_t audioGetQueue_;
    TaskHandle_t taskHandle_;
    Audio audio_;
};

extern AudioTaskControl audioControl; // Universal AudioTaskControl