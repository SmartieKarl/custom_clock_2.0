#pragma once

#include <Arduino.h>
#include "config.h"

// blynk_client.h
// Hosts a FreeRTOS task that manages the Blynk connection and callbacks.

class BlynkClient
{
  public:
    BlynkClient();

    bool begin();
    bool connect();
    void connectViaTaskRunner();

  private:
    TaskHandle_t taskHandle_;

  public:
    void handleVirtualWrite_0_(const char *param);

    void handleConnected_();

  private:
    static void taskRunner_(void *pvParameters);
};

extern BlynkClient blynkClient; // Universal Blynk client