#include <Arduino.h>
#include <MFRC522.h>
#include <RTClib.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <lvgl.h>

#include <Audio.h>

#include <FastLED.h>

#include "alarm_system.h"
#include "audio_control.h"
#include "blynk_client.h"
#include "brightness_control.h"
#include "command_interface.h"
#include "config.h"
#include "led_config.h"
#include "log.h"
#include "network_manager.h"
#include "rfid_control.h"
#include "scheduler.h"
#include "time_sync.h"
#include "timekeeper.h"
#include "ui.h"
#include "weather_sync.h"

// Hardware objects
TFT_eSPI tft = TFT_eSPI();
MFRC522 rfid(Pins::RFID_CS, Pins::RFID_RST);
RTC_DS3231 rtc;
CRGB leds[NUM_LEDS];

//========================================
// LVGL stuff
//========================================
#include "esp_heap_caps.h"

static lv_disp_draw_buf_t draw_buf;

#define DRAW_BUFFER_PIXELS (SCREEN_WIDTH * SCREEN_HEIGHT / 4)

static lv_color_t *buf1;
static lv_color_t *buf2;

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixelsDMA((uint16_t *)color_p, w * h);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    // TOUCH_IRQ pulls low when touch is registered
    bool touched = digitalRead(Pins::TOUCH_IRQ) == LOW;

    if (!touched)
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        uint16_t touchX, touchY;
        tft.getTouch(&touchX, &touchY, 600);

        data->state = LV_INDEV_STATE_PR;

        /*Set the coordinates*/
        data->point.x = touchX;
        data->point.y = (SCREEN_HEIGHT - 1) - touchY;
    }
}
//========================================
// End LVGL stuff
//========================================

// Switch to help debug reason for crash/reset
const char *resetReasonToString(esp_reset_reason_t reason)
{
    switch (reason)
    {
    case ESP_RST_UNKNOWN:
        return "Unknown";

    case ESP_RST_POWERON:
        return "Power-on reset";

    case ESP_RST_EXT:
        return "External pin reset";

    case ESP_RST_SW:
        return "Software reset";

    case ESP_RST_PANIC:
        return "Exception/panic reset";

    case ESP_RST_INT_WDT:
        return "Interrupt watchdog reset";

    case ESP_RST_TASK_WDT:
        return "Task watchdog reset";

    case ESP_RST_WDT:
        return "Other watchdog reset";

    case ESP_RST_DEEPSLEEP:
        return "Wake from deep sleep";

    case ESP_RST_BROWNOUT:
        return "Brownout reset";

    case ESP_RST_SDIO:
        return "SDIO reset";

    default:
        return "Invalid/reset reason not recognized";
    }
}

void logResetReason()
{
    esp_reset_reason_t reason = esp_reset_reason();

    LOG.log(
        "\nREBOOT\n"
        "Reset code: %d\n"
        "Reason: %s",
        int(reason),
        resetReasonToString(reason));
}

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

    configTzTime(TIME_ZONE, "pool.ntp.org", "time.nist.gov");

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
    tft.initDMA();

    uint16_t calData[5] = {375, 3454, 438, 3143, 7};
    // tft.setTouch(calData);
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

    LOG.begin();
    logResetReason();

    if (!alarmSystem.begin(rtc))
    {
        printTerminalMessage("Alarm System failed to start.", TFT_RED);
        fail = true;
    }
    rfidControl.begin(rfid);
    brightnessControl.begin();

    // Network stuff
    networkManager.begin();

    if (!timeSync.begin(rtc))
    {
        printTerminalMessage("timeSync failed to start.", TFT_RED);
        fail = true;
    }
    else
        timeSync.syncViaTaskRunner();
    if (!weatherSync.begin())
    {
        printTerminalMessage("weatherSync failed to start.", TFT_RED);
        fail = true;
    }
    else
        weatherSync.syncViaTaskRunner();
    if (!blynkClient.begin())
    {
        printTerminalMessage("blynkClient failed to start.", TFT_RED);
        fail = true;
    }
    else
        blynkClient.connectViaTaskRunner();

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

    Serial.println("Starting LVGL...");
    // LVGL
    lv_init();

    buf1 = (lv_color_t *)heap_caps_malloc(
        DRAW_BUFFER_PIXELS * sizeof(lv_color_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    buf2 = (lv_color_t *)heap_caps_malloc(
        DRAW_BUFFER_PIXELS * sizeof(lv_color_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (buf1 == nullptr || buf2 == nullptr)
    {
        Serial.println("Failed to allocate LVGL draw buffers!");
        while (true)
            delay(1000);
    }

    lv_disp_draw_buf_init(
        &draw_buf,
        buf1,
        buf2,
        DRAW_BUFFER_PIXELS);

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;

    lv_disp_drv_register(&disp_drv);

    /*Initialize the input device driver*/
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // UI
    ui_init();
    Serial.println("Setup complete.");
}

void loop()
{
    lv_timer_handler(); // Let GUI do it's work

    commandInterface.handleSerialIn();
    brightnessControl.updateAmbient();

    if (timekeeper.tick()) // Secondly updates
    {
        /*
        Run UI updates, update clock screen, etc. here
        */

        scheduler.run();
    }
    yield();
}

/*======================================================================
                                TODO
======================================================================

===== ASAP =====
BACKEND
- Add a screen brightness controller
- Get Log system fully integrated

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