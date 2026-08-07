#pragma once

#include <Arduino.h>
#include <MFRC522.h>

// rfid_control.h
// RFID hardware abstraction layer for simpler card reading and ID

class RfidControl
{
  public:
    RfidControl();

    void begin(MFRC522 &rfid);

    void poll(char out[64]);
    void getCardUID(char out[64]);

  private:
    MFRC522 *rfid_;
};

extern RfidControl rfidControl; // Universal RfidControl