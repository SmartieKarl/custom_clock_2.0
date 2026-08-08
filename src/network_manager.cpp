#include "network_manager.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFi.h>

NetworkManager networkManager; // Global shared instance

NetworkManager::NetworkManager()
    : users_(0), persistent_(false), connecting_(false), mutex_(NULL)
{
}

void NetworkManager::begin()
{
    mutex_ = xSemaphoreCreateMutex();
}

bool NetworkManager::startWiFiSession()
{
    // 30 sec timeout to prevent deadlock
    // This is the queue if another user is using the wifi
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10000)) != pdTRUE)
        return false;

    unsigned long waitStart = millis();
    while (connecting_)
    {
        if (millis() - waitStart > 30000) // Timeout after 30s
        {
            xSemaphoreGive(mutex_);
            return false;
        }

        // Release mutex while waiting
        xSemaphoreGive(mutex_);
        vTaskDelay(pdMS_TO_TICKS(50));

        // Re-acquire
        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10000)) != pdTRUE)
            return false;
    }

    if (users_ == 0 && WiFi.status() != WL_CONNECTED)
    {
        connecting_ = true;
        WiFi.mode(WIFI_STA);
        WiFi.begin(Secret::WIFI_SSID, Secret::WIFI_PASSWORD);

        unsigned long start = millis();

        xSemaphoreGive(mutex_);
        while (WiFi.status() != WL_CONNECTED && (millis() - start < 15000))
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // Re-acquire mutex and update connecting_
        xSemaphoreTake(mutex_, pdMS_TO_TICKS(10000));
        connecting_ = false;

        if (WiFi.status() != WL_CONNECTED)
        {
            xSemaphoreGive(mutex_);
            return false;
        }
    }

    users_++;
    xSemaphoreGive(mutex_);
    return true;
}

void NetworkManager::endWiFiSession()
{
    xSemaphoreTake(mutex_, pdMS_TO_TICKS(10000));
    if (users_ > 0)
        users_--;

    // Disconnect if no users and not in persistent mode
    if (users_ == 0 && !persistent_)
    {
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
    }

    xSemaphoreGive(mutex_);
}

void NetworkManager::setWiFiPersistent(bool persistent)
{
    xSemaphoreTake(mutex_, pdMS_TO_TICKS(10000));
    persistent_ = persistent;
    Serial.print("WiFi persistent mode: ");
    Serial.println(persistent ? "ENABLED" : "DISABLED");
    xSemaphoreGive(mutex_);
}

bool NetworkManager::isWiFiPersistent() const
{
    xSemaphoreTake(mutex_, pdMS_TO_TICKS(10000));
    bool result = persistent_;
    xSemaphoreGive(mutex_);
    return result;
}