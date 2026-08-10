// #define BLYNK_DEBUG
// #define BLYNK_PRINT Serial
#include "blynk_client.h"
#include "command_interface.h"
#include "config.h"
#include "network_manager.h"
#include "scheduler.h"
#include "log.h"

#include <BlynkSimpleEsp32.h>

BlynkClient blynkClient; // Global shared instance

BlynkClient::BlynkClient()
    : taskHandle_(NULL)
{
}

bool BlynkClient::begin()
{
    if (!scheduler.registerCallback(
            "blynkClient",
            [](void *context)
            {
                static_cast<BlynkClient *>(context)->connectViaTaskRunner();
            },
            this))
        return false;

    Blynk.config(BLYNK_AUTH);

    xTaskCreatePinnedToCore(
        taskRunner_,
        "BlynkClientTask",
        4096,
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

    if (persistent)
    {
        while (networkManager.isWiFiPersistent())
        {
            Blynk.run();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        Serial.println("BlynkClient: persistence ended, closing WiFi.");
    }
    else
    {
        const unsigned long start = millis();
        while (millis() - start < BURST_MS)
        {
            Blynk.run();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
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

void BlynkClient::handleVirtualWrite_(const char *value)
{
    Serial.println("BlynkClient: virtual write received");
    const char *rsp = commandInterface.handleBlynkIn(value);
    if (rsp[0] != '\0') // Will be '\0' if the read command had the signature clock prefix: [CLK]:
    {
        // Write and notify
        Blynk.virtualWrite(V0, rsp);
        Blynk.logEvent("clock_reply", rsp);
        Serial.printf("rsp <%s> written to pin V0.", rsp);
    }
}

void BlynkClient::handleConnected_()
{
    Serial.println("BlynkClient: connected");
    Blynk.sendCmd(BLYNK_CMD_PING);
    Blynk.syncVirtual(V0);

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

BLYNK_WRITE(V0)
{
    blynkClient.handleVirtualWrite_(param.asStr());
}

BLYNK_CONNECTED()
{
    blynkClient.handleConnected_();
}