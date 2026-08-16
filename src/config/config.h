#pragma once

#include "secret.h"
#include <stdint.h>

// config.h
// useful globals definitions here

namespace Pins
{
constexpr uint8_t TOUCH_IRQ = 6;

// FSPI (TFT_eSPI uses HSPI and doesn't play nice with other devices)
constexpr uint8_t FSPI_SCK = 48;
constexpr uint8_t FSPI_MOSI = 47;
constexpr uint8_t FSPI_MISO = 21;

// I2C
constexpr uint8_t I2C_SDA = 16;
constexpr uint8_t I2C_SCL = 17;

// RFID-RC522
constexpr uint8_t RFID_CS = 40;
constexpr uint8_t RFID_RST = 39;

// DS3231
constexpr uint8_t DS3231_SQW = 18;

// MAX98357A
constexpr uint8_t AMP_BCLK = 3; // bit clock
constexpr uint8_t AMP_LRC = 15; // left-right clock
constexpr uint8_t AMP_DIN = 2;  // data in
constexpr uint8_t AMP_SD = 42;  // shutdown

// MicroSD card reader
constexpr uint8_t SD_CS = 5;

// RGB strip control
constexpr uint8_t RGB_DIN = 41;

// TFT backlight control
constexpr uint8_t PHOTORESISTOR_IN = 1;
constexpr uint8_t TFT_BACKLIGHT = 38;
} // namespace Pins

/*
========================================
              IMPORTANT
========================================
 This code requires a secret.h file that contains the following namespace inside:

// WiFi and Clock sync settings
constexpr const char *WIFI_SSID = "ssid";
constexpr const char *WIFI_PASSWORD = "password";
constexpr const char *TIME_ZONE = "time.zone"; // in POSIX time zone format

// Weather API settings (API key from OpenWeatherMap)
constexpr const char *WEATHER_API_KEY = "abcdef";
constexpr const char *WEATHER_LOCATION = "City,ST,US";

// Master NFC tag UID
constexpr const char *ALARM_CARD_UID = "12345";

// Blynk inegration
#define *BLYNK_TEMPLATE_ID "tempID"
#define *BLYNK_TEMPLATE_NAME "Custom Clock 2"
#define *BLYNK_AUTH "auth code" // Individual to device
*/