#pragma once

// network_manager.h
// Thread-safe, client-based network connection manager module

class NetworkManager
{
  public:
    NetworkManager();

    void begin();

    bool startWiFiSession();
    void endWiFiSession();

    // WiFi persistence control (persistent keeps WiFi connected)
    void setWiFiPersistent(bool persistent);
    bool isWiFiPersistent() const;

  private:
    uint8_t users_;
    bool persistent_;
    bool connecting_;

    mutable SemaphoreHandle_t mutex_; // Mutex safety
};

extern NetworkManager networkManager; // Universal NetworkManager