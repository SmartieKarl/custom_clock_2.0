#include "rfid_control.h"

RfidControl rfidControl; // Global shared instance

// Constructor
RfidControl::RfidControl()
    : rfid_(nullptr)
{
}

void RfidControl::begin(MFRC522 &rfid)
{
    rfid_ = &rfid;
}

// Polls the RFID reader for a card. If a card is found,
// pass the UID into an input buffer.
void RfidControl::poll(char out[64])
{
    if (!rfid_->PICC_IsNewCardPresent() ||
        !rfid_->PICC_ReadCardSerial())
    {
        out[0] = '\0';
        return;
    }

    getCardUID(out);

    rfid_->PICC_HaltA();
    rfid_->PCD_StopCrypto1();
}

// Get card UID as formatted buffer
void RfidControl::getCardUID(char out[64])
{
    byte uidLen = rfid_->uid.size;
    if(uidLen == 0 || uidLen > 10) {
        out[0] = '\0';
        return;
    }

    char* ptr = out;
    size_t remaining = 64;

    for(byte i = 0; i < uidLen; i++) {
        int written = snprintf(ptr, remaining, "%02X", rfid_->uid.uidByte[i]);
        if(written < 0 || written >= remaining)
            break;
        ptr += written;
        remaining -= written;
    }

    *ptr = '\0';
}