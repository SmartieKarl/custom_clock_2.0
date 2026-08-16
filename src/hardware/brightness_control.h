#pragma once
#include "config.h"
#include <Arduino.h>

// brightness_control.h
// Controls TFT backlight brightness, dimming/brightening based on ambient light.

namespace BrightnessConfig
{
constexpr uint8_t BRIGHTNESS_MIN = 1;   // Minimum backlight brightness
constexpr uint8_t BRIGHTNESS_MAX = 255; // Maximum backlight brightness

constexpr uint16_t LIGHT_SENSOR_MIN = 120;       // ADC reading for darkest environment
constexpr uint16_t LIGHT_SENSOR_MAX = 1200;      // ADC reading for brightest environment
constexpr uint32_t LIGHT_UPDATE_INTERVAL = 5000; // How often to re-check ambient light (ms)

constexpr uint8_t FADE_STEP_SIZE = 2;   // Brightness change per fade step
constexpr uint32_t FADE_STEP_DELAY = 5; // Delay between fade steps (ms)

// Fixed PWM settings - not expected to change per-instance
constexpr uint8_t PWM_CHANNEL = 0;
constexpr uint32_t PWM_FREQUENCY = 5000;
constexpr uint8_t PWM_RESOLUTION = 8;
} // namespace BrightnessConfig

class BrightnessControl
{
  public:
    BrightnessControl(uint8_t pwmPin = Pins::TFT_BACKLIGHT,
                         uint8_t photoresistorPin = Pins::PHOTORESISTOR_IN);

    void begin();

    void setBrightness(uint8_t level);
    void updateAmbient(); // call this regularly from loop()

    uint8_t getBrightness() const { return currentBrightness_; }
    bool isFading() const { return currentBrightness_ != targetBrightness_; }

  private:
    uint16_t readLightSensor() const;
    void updateFade();

    uint8_t pwmPin_;
    uint8_t photoresistorPin_;

    uint8_t currentBrightness_;
    uint8_t targetBrightness_;

    unsigned long lastFadeUpdate_;
    unsigned long lastAmbientUpdate_;
};

extern BrightnessControl brightnessControl; // Universal BrightnessControl