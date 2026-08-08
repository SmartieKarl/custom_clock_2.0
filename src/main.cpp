#include <Arduino.h>
#include <MFRC522.h>
#include <RTClib.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Wire.h>

#include <Audio.h>

#include <FastLED.h>

#include "alarm_system.h"
#include "audio_control.h"
#include "blynk_client.h"
#include "config.h"
#include "rfid_control.h"
#include "scheduler.h"
#include "time_sync.h"
#include "timekeeper.h"
#include "weather_sync.h"

TFT_eSPI tft = TFT_eSPI();
MFRC522 rfid(Pins::RFID_CS, Pins::RFID_RST);
RTC_DS3231 rtc;
CRGB leds[NUM_LEDS];

void printTerminalMessage(const char *message, uint16_t color = TFT_WHITE)
{
    tft.setTextColor(color);
    tft.println(message);
}

/*============================================================
                            SETUP
============================================================*/
void setup()
{
    Serial.begin(115200);
    delay(100);

    // ===== Configure GPIOs =====
    pinMode(Pins::AMP_SD, OUTPUT);
    digitalWrite(Pins::AMP_SD, LOW); // Mute amplifier

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    pinMode(Pins::TOUCH_IRQ, INPUT_PULLUP);

    // Set CS's to known state
    pinMode(TFT_CS, OUTPUT);
    pinMode(Pins::SD_CS, OUTPUT);
    pinMode(Pins::RFID_CS, OUTPUT);
    pinMode(TOUCH_CS, OUTPUT);

    digitalWrite(TFT_CS, HIGH);
    digitalWrite(Pins::SD_CS, HIGH);
    digitalWrite(Pins::RFID_CS, HIGH);
    digitalWrite(TOUCH_CS, HIGH);

    // ===== Board Init =====
    bool fail = false, wait = false;
    // TFT
    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    tft.setTextFont(1);
    tft.setCursor(0, 0);
    printTerminalMessage("- Michael's totally wicked custom clock 2.0 -\n");

    // SPI (FSPI alt bus)
    SPI.begin(Pins::FSPI_SCK, Pins::FSPI_MISO, Pins::FSPI_MOSI, -1);

    // RFID
    rfid.PCD_Init();
    rfid.PCD_AntennaOn();
    byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
    if (version == 0x00 || version == 0xFF)
    {
        printTerminalMessage("RFID module failed to respond.", TFT_RED);
        fail = true;
    }

    // MicroSD
    if (!SD.begin(Pins::SD_CS, SPI))
    {
        printTerminalMessage("MicroSD Card module failed to respond.", TFT_RED);
        fail = true;
    }

    // I2C
    if (!Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL))
    {
        printTerminalMessage("I2C failed to initialize.", TFT_RED);
        fail = true;
    }

    // RTC
    if (!rtc.begin())
    {
        printTerminalMessage("RTC module failed to respond.", TFT_RED);
        fail = true;
    }
    else
    {
        if (rtc.lostPower())
        {
            printTerminalMessage("RTC module experienced a power loss. Resetting time...", TFT_YELLOW);
            rtc.adjust(DateTime(0, 1, 1, 0, 0, 0));
            rtc.setAlarm1(DateTime(0, 0, 0, 0, 0, 0), DS3231_A1_Hour);
            rtc.disableAlarm(1);
            wait = true;
        }
    }

    // RGBW LEDs
    FastLED.addLeds<SK6812, Pins::RGB_DIN, GRB>(leds, NUM_LEDS).setRgbw(RgbwDefault());
    FastLED.setBrightness(255);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 1500); // 5v 1500mA
    FastLED.clear();
    FastLED.show();

    // ===== Services Init =====
    audioControl.begin(0, 3); // Core 0 and highest priority to avoid streaming lag.
    timekeeper.begin(rtc);
    if (!alarmSystem.begin(rtc))
    {
        printTerminalMessage("Alarm System failed to start.", TFT_RED);
        fail = true;
    }
    rfidControl.begin(rfid);

    // Network stuff
    if (!timeSync.begin(rtc))
    {
        printTerminalMessage("timeSync failed to start.", TFT_RED);
        fail = true;
    }
    if (weatherSync.begin())
    {
        printTerminalMessage("timeSync failed to start.", TFT_RED);
        fail = true;
    }
    if (blynkClient.begin())
    {
        printTerminalMessage("timeSync failed to start.", TFT_RED);
        fail = true;
    }

    // Flag handling
    if (fail)
    {
        printTerminalMessage("\nWarning: something failed during startup.\nThe device will not work as intended!", TFT_YELLOW);
        wait = true;
    }
    if (wait)
    {
        printTerminalMessage("\nTap anywhere to continue...");
        while (digitalRead(Pins::TOUCH_IRQ) == HIGH)
            ; // Wait until t_irq pulls low
    }

    /*
    Start UI, switch to main clock screen, etc.
    */
}

constexpr int touchPollFrequency = 1000 / 30; // ms
uint32_t lastTouchPoll = 0;
void loop()
{
    if (timekeeper.tick()) // Secondly updates
    {
        /*
        Run UI updates, update clock screen, etc. here
        */

        scheduler.run();
    }

    if (millis() - lastTouchPoll > touchPollFrequency)
    {
        lastTouchPoll = millis();

        if (digitalRead(Pins::TOUCH_IRQ) == LOW)
        {
            /*
            Run touch logic here
            */
            int foo;
        }
    }
}

/*======================================================================
                                TODO
======================================================================

===== ASAP =====
BACKEND
- Setup blynk stuff, get comms to it functional
- Add a screen brightness controller

FRONTEND
- Export and integrate squareline studio UI


===== EVENTUALLY =====
BACKEND
- RGBW LED functions
- Comms to light bar on wall of room?
- Log system?
- Command system?
- Link to computer to view SD files? Boot mode switching for this?

FRONTEND
- Settings menu, weather sprites, etc
- CAD design: Case and material selection
- PCB design: one-sided? two-sided? dims?
*/