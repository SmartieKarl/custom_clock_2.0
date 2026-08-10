#pragma once

#include <Arduino.h>

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
    void handleVirtualWrite_(const char *value);
    void handleConnected_();

  private:

    static void taskRunner_(void *pvParameters);
};

extern BlynkClient blynkClient; // Universal Blynk client