#include "brightness_control.h"

using namespace BrightnessConfig;

BrightnessControl brightnessControl;

BrightnessControl::BrightnessControl(uint8_t pwmPin, uint8_t photoresistorPin)
    : pwmPin_(pwmPin),
      photoresistorPin_(photoresistorPin),
      currentBrightness_(BRIGHTNESS_MAX),
      targetBrightness_(BRIGHTNESS_MAX),
      lastFadeUpdate_(0),
      lastAmbientUpdate_(0)
{
}

void BrightnessControl::begin()
{
    ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(pwmPin_, PWM_CHANNEL);
    pinMode(photoresistorPin_, INPUT);

    setBrightness(BRIGHTNESS_MAX);
}

void BrightnessControl::setBrightness(uint8_t level)
{
    level = constrain(level, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
    currentBrightness_ = level;
    targetBrightness_ = level;
    ledcWrite(PWM_CHANNEL, currentBrightness_);
}

uint16_t BrightnessControl::readLightSensor() const
{
    return analogRead(photoresistorPin_);
}

void BrightnessControl::updateFade()
{
    if (currentBrightness_ == targetBrightness_)
        return;

    unsigned long now = millis();
    if (now - lastFadeUpdate_ < FADE_STEP_DELAY)
        return;

    int diff = (int)targetBrightness_ - (int)currentBrightness_;
    int step = min((int)FADE_STEP_SIZE, abs(diff));
    currentBrightness_ += (diff > 0) ? step : -step;

    ledcWrite(PWM_CHANNEL, currentBrightness_);
    lastFadeUpdate_ = now;
}

void BrightnessControl::updateAmbient()
{
    updateFade();

    unsigned long now = millis();
    if (now - lastAmbientUpdate_ < LIGHT_UPDATE_INTERVAL)
        return;
    lastAmbientUpdate_ = now;

    uint16_t light = constrain(readLightSensor(), LIGHT_SENSOR_MIN, LIGHT_SENSOR_MAX);
    targetBrightness_ = map(light, LIGHT_SENSOR_MIN, LIGHT_SENSOR_MAX,
                             BRIGHTNESS_MIN, BRIGHTNESS_MAX);
}