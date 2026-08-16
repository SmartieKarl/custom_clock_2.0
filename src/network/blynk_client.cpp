// #define BLYNK_DEBUG
// #define BLYNK_PRINT Serial
#include "blynk_client.h"
#include "command_interface.h"
#include "led_config.h"
#include "log.h"
#include "network_manager.h"
#include "scheduler.h"

#include <BlynkSimpleEsp32.h>
#include <FastLED.h>

BlynkClient blynkClient; // Global shared instance

extern CRGB leds[NUM_LEDS];

BlynkClient::BlynkClient()
    : taskHandle_(NULL)
{
}

bool BlynkClient::begin()
{
    Serial.println("BlynkClient: begin() called.");
    if (!scheduler.registerCallback(
            "blynkClient",
            [](void *context)
            {
                static_cast<BlynkClient *>(context)->connectViaTaskRunner();
            },
            this))
    {
        Serial.println("BlynkClient: Failed to start!");
        return false;
    }

    Blynk.config(BLYNK_AUTH);

    xTaskCreatePinnedToCore(
        taskRunner_,
        "BlynkClientTask",
        8192,
        this,
        1,
        &taskHandle_,
        0);

    return taskHandle_ != NULL;
}

bool BlynkClient::connect()
{
    constexpr int MAX_RETRIES = 5;
    constexpr unsigned long BURST_MS = 20000; // How long the ocnnect session will stay open

    const bool persistent = networkManager.isWiFiPersistent();
    bool connected = false;
    const int retries = persistent ? 1 : MAX_RETRIES;

    for (int i = 0; i < retries && !connected; ++i)
    {
        Serial.println("BlynkClient: attempting to connect to WiFi");
        if (networkManager.startWiFiSession())
        {
            Serial.println("BlynkClient: WiFi connected successfully.");
            connected = true;
        }
        else if (!persistent)
        {
            Serial.printf("BlynkClient: WiFi failed. Retrying... (%d/%d)\n", i + 1, MAX_RETRIES);
        }
    }

    if (!connected)
    {
        Serial.println("BlynkClient: failed to connect to Blynk cloud.");
        return false;
    }

    Blynk.connect(10000);

    if (!Blynk.connected())
        return false;

    const unsigned long start = millis();
    while (networkManager.isWiFiPersistent() || millis() - start < BURST_MS)
    {
        Blynk.run();
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    Blynk.disconnect();
    networkManager.endWiFiSession();
    Serial.println("BlynkClient: WiFi session ended.");
    return true;
}

// Notifies the taskRunner to run sync(). Recommended to prevent blocking.
void BlynkClient::connectViaTaskRunner()
{
    if (taskHandle_ != NULL)
        xTaskNotifyGive(taskHandle_);
}

void BlynkClient::taskRunner_(void *pvParameters)
{
    BlynkClient *instance = static_cast<BlynkClient *>(pvParameters);

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        instance->connect();
    }
}

// Blynk macro compatability layers

// cmd stream
void BlynkClient::handleVirtualWrite_0_(const char *param)
{
    Serial.println("BlynkClient: virtual write received");
    const char *rsp = commandInterface.handleBlynkIn(param);
    if (rsp[0] != '\0') // Will be '\0' if the read command had the signature clock prefix: [CLK]:
    {
        // Write and notify
        Blynk.virtualWrite(V0, rsp);
        Blynk.logEvent("clock_reply", rsp);
        Serial.printf("rsp <%s> written to pin V0.", rsp);
    }
}

// On connect
void BlynkClient::handleConnected_()
{
    Serial.println("BlynkClient: connected");
    Blynk.sendCmd(BLYNK_CMD_PING);
    Blynk.syncAll();

    // Log flush
    char buf[LOG_ENTRY_SIZE];

    String summary;

    while (LOG.pop(buf))
    {
        summary += buf;
        summary += "\n";
    }

    if (summary.length())
    {
        Blynk.virtualWrite(V1, summary);
        Blynk.logEvent("clock_log", summary);
    }
}

// Command stream macro
BLYNK_WRITE(V0)
{
    blynkClient.handleVirtualWrite_0_(param.asStr());
}

// V1 is the Log output stream. Not read

// LED color stream macro (param string, 3 seperate color values)
BLYNK_WRITE(V2)
{
    int r = param[0].asInt();
    int g = param[1].asInt();
    int b = param[2].asInt();

    ledConfig.color = CRGB(r, g, b);

    fill_solid(leds, NUM_LEDS, ledConfig.color);
    FastLED.show();
}

// LED toggle stream macro (param 0-1)
BLYNK_WRITE(V3)
{
    bool ledsOn = param.asInt(); // data stream should only accept 1 or 0

    if (ledsOn)
    {
        ledConfig.override = true;
        fill_solid(leds, NUM_LEDS, ledConfig.color);
        FastLED.setBrightness(ledConfig.brightness);
        FastLED.show();
    }
    else
    {
        ledConfig.override = false;
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();
    }
}

// LED brightness stream macro (param 0-100)
BLYNK_WRITE(V4)
{
    ledConfig.brightness = map(param.asInt(), 0, 100, 0, 255);

    if (ledConfig.override)
    {
        FastLED.setBrightness(ledConfig.brightness);
        FastLED.show();
    }
}

// LED animation speed stream macro (param 0-10000)
BLYNK_WRITE(V5)
{
    ledConfig.animationSpeedMs = param.asInt();

    // Future logic for animations here
}

// Blynk on connect macro
BLYNK_CONNECTED()
{
    blynkClient.handleConnected_();
}