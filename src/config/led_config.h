#pragma once

#include <FastLED.h>
#include <stdint.h>

// led_config.h
// LED strip user configuration. Updates when user controls the light colors

static constexpr uint8_t NUM_LEDS = 144;

struct LEDConfig
{
    bool override = false;
    CRGB color = CRGB::White;
    uint8_t brightness = 255;
    unsigned int animationSpeedMs = 0;
};

extern LEDConfig ledConfig; // Universal LEDConfig