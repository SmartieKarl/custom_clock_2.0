// Test file to make sure all peripherals of the project are working as needed.

#include <Arduino.h> // Arduino core library
#include <SD.h>      // SD card support
#include <SPI.h>     // SPI protocol support
#include <Wire.h>    // I2C protocol support

#include <MFRC522.h>  // RFID-RC522 core library
#include <RTClib.h>   // DS3231 RTC core library
#include <TFT_eSPI.h> // ST7789 TFT core library

#include <Audio.h> // ESP32-audioI2S core library

#include <FastLED.h> // For the SK6812 RGBW LED strip

// Pin defs
// TFT pins are ddefined in custom_clock_user_setup.cpp

// FSPI (TFT_eSPI uses HSPI and doesn't play nice with other devices)
#define FSPI_SCK 48
#define FSPI_MOSI 47
#define FSPI_MISO 21

// I2C
#define I2C_SDA 16
#define I2C_SCL 17

// RFID-RC522
#define RFID_CS 40
#define RFID_RST 39

// DS3231
#define DS3231_SQW 18

// MAX98357A amp
#define AMP_BCLK 1 // bit clock
#define AMP_LRC 15 // left-right clock
#define AMP_DIN 2  // data in
#define AMP_SD 42  // shutdown

// MicroSD card reader
#define SD_CS 5

// RGB light control
#define RGB_DIN 41

// add'l config
#define NUM_LEDS 144

// Objects
TFT_eSPI tft = TFT_eSPI();
MFRC522 rfid(RFID_CS, RFID_RST);
RTC_DS3231 rtc;
Audio audio;
CRGB leds[NUM_LEDS];

// rtc sqw interrupt prep
volatile bool rtcTick = false;

void IRAM_ATTR rtcTickISR()
{
    rtcTick = true;
}

// Function defs
void listDirectory(fs::FS &fs, const char *dirname, uint8_t levels);

// Peripheral fail state flags
bool wireOK = false;
bool sdOK = false;
bool rfidOK = false;
bool rtcOK = false;
bool audioOK = false;
void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("Boot: serial online");

    // 1. Configure GPIOs
    pinMode(AMP_SD, OUTPUT);
    digitalWrite(AMP_SD, LOW); // Mute amplifier while starting up

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // Set CS's to known state
    pinMode(TFT_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    pinMode(RFID_CS, OUTPUT);
    pinMode(TOUCH_CS, OUTPUT);

    digitalWrite(TFT_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    digitalWrite(RFID_CS, HIGH);
    digitalWrite(TOUCH_CS, HIGH);

    // Set sqw to input
    pinMode(DS3231_SQW, INPUT_PULLUP);

    // 3. Display
    Serial.println("Boot: initializing display");
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    Serial.println("Boot: display ready");

    SPI.begin(FSPI_SCK, FSPI_MISO, FSPI_MOSI, -1);

    // 5. Hardware reset RC522 to ensure clean boot state
    pinMode(RFID_RST, OUTPUT);
    digitalWrite(RFID_RST, LOW);
    delay(50);
    digitalWrite(RFID_RST, HIGH);
    delay(50);

    // 6. Initialize RFID using extSPI
    Serial.println("Boot: Initializing RFID...");
    rfid.PCD_Init(); // Pass pointer to secondary SPI instance
    rfid.PCD_AntennaOn();

    byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
    Serial.printf("RC522 Version: 0x%02X\n", version);
    if (!(version == 0x00 || version == 0xFF))
    {
        rfidOK = true;
    }

    // 7. Initialize SD Card using SPI
    Serial.println("Boot: Initializing SD Card...");
    sdOK = SD.begin(SD_CS, SPI); // Pass extSPI instance to SD library
    Serial.printf("Boot: SD.begin returned %d\n", sdOK);

    wireOK = Wire.begin(I2C_SDA, I2C_SCL); // Start I2C
    Serial.printf("Boot: Wire.begin returned %d\n", wireOK);

    // 8. RTC
    Serial.println("Boot: initializing RTC");
    rtcOK = rtc.begin();
    if (rtcOK)
    {
        if (rtc.lostPower())
            Serial.println("Boot: RTC experienced a power loss.");
        rtc.writeSqwPinMode(DS3231_SquareWave1Hz); // Configure sqw pin
        attachInterrupt(
            digitalPinToInterrupt(DS3231_SQW),
            rtcTickISR,
            FALLING); // set sqw interrupt
    }

    // 7. RGB LEDs
    Serial.println("Boot: initializing LEDs");
    FastLED.addLeds<SK6812, RGB_DIN, GRB>(leds, NUM_LEDS).setRgbw(RgbwDefault());
    FastLED.setBrightness(255);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 1500);
    FastLED.clear();
    FastLED.show();
    Serial.println("Boot: LEDs ready");

    // 8. Audio
    Serial.println("Boot: initializing audio");
    audioOK = audio.setPinout(AMP_BCLK, AMP_LRC, AMP_DIN);
    audio.setBufsize(0, 32768); // 32kb in psram, ~1,5s audio/buf
    audio.setVolume(17);
    Serial.printf("Boot: audio.setPinout returned %d\n", audioOK);

    digitalWrite(AMP_SD, HIGH); // Enable MAX98357A

    // Begin testing
    Serial.println("--------------------");
    Serial.println("Beginning diagnostics...");
    Serial.println("--------------------\n");
    Serial.println("Startup flags:");
    Serial.printf("wireOK: %d\nsdOK: %d\nrfidOK: %d\nrtcOK: %d\naudioOK: %d\n", wireOK, sdOK, rfidOK, rtcOK, audioOK);
    Serial.println("\nNote: SPI, display, and leds do not return a bool on their init functions.");

    // I2C scan
    Serial.println("Now printing all recognized I2C addresses...");
    Serial.println("Scanning I2C bus...");

    byte count = 0;

    for (byte address = 1; address < 127; address++)
    {
        Wire.beginTransmission(address);

        if (Wire.endTransmission() == 0)
        {
            Serial.printf("Found device at 0x%02X\n", address);
            count++;
        }
    }

    if (count == 0)
        Serial.println("No I2C devices found.");
    else
        Serial.printf("Found %d I2C device(s).\n", count);

    // Display color test
    Serial.println("\nTesting display colors in 5s...");
    delay(5000);
    tft.fillScreen(TFT_RED);
    Serial.println("RED");
    delay(1000);
    tft.fillScreen(TFT_GREEN);
    Serial.println("GREEN");
    delay(1000);
    tft.fillScreen(TFT_BLUE);
    Serial.println("BLUE");
    delay(1000);
    Serial.println("Test complete.");

    // LED strip color test
    Serial.println("Testing LED strip in 5s...");
    delay(5000);
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    Serial.println("RED");
    delay(1000);

    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.show();
    Serial.println("GREEN");
    delay(1000);

    fill_solid(leds, NUM_LEDS, CRGB::Blue);
    Serial.println("BLUE");
    FastLED.show();
    delay(1000);

    fill_solid(leds, NUM_LEDS, CRGB::White);
    Serial.println("WHITE");
    FastLED.show();
    Serial.println("Test complete.");

    // SD card read
    if (sdOK)
    {
        Serial.println("Reading SD card directory...");
        listDirectory(SD, "/", 10);
        Serial.println("Test complete.");
    }
    else
    {
        Serial.println("No SD card detected, skipping directory read.");
    }

    // RTC time read
    if (rtcOK)
    {
        Serial.println("\nTesting RTC functionality...");
        DateTime now = rtc.now();
        Serial.printf("Current time: %02d/%02d/%d %02d:%02d:%02d\n", now.month(), now.day(), now.year(), now.hour(), now.minute(), now.second());
        delay(1000);
        now = rtc.now();
        Serial.printf("Current time: %02d/%02d/%d %02d:%02d:%02d\n", now.month(), now.day(), now.year(), now.hour(), now.minute(), now.second());
        delay(1000);
        now = rtc.now();
        Serial.printf("Current time: %02d/%02d/%d %02d:%02d:%02d\n", now.month(), now.day(), now.year(), now.hour(), now.minute(), now.second());
        Serial.println("Test complete.");
    }
    else
        Serial.println("Cannot test RTC functionality because no RTC found.");

    Serial.println("\nBeginning loop tests.");
    if (rfidOK)
        Serial.println("Test RFID by holding a scannable chip next to it.");
    if (rtcOK)
        Serial.println("DS3231's SQW pin will trigger a Serial print on falling edge.");
    if (audioOK && sdOK)
    {
        Serial.println("Audio will read an mp3 file from the mounted SD card and play it via the amp.");
        Serial.println("Opening /test.mp3 for playback...");
        if (audio.connecttoFS(SD, "/test.mp3")) // open the file and begin playback
            Serial.println("Playing /test.mp3...");
        else
            Serial.println("couldn't open file!");
    }
    else if (!sdOK)
    {
        Serial.println("Skipping audio test because SD card isn't mounted.");
    }
}

unsigned long lastRfidCheck = 0;
const unsigned long rfidPollInterval = 100; // ms
void loop()
{
    if (audioOK)
        audio.loop(); // Maintain audio system

    if (rtcOK && rtcTick)
    {

        rtcTick = false; // reset flag
        Serial.println("\nRTC tick!");
        DateTime now = rtc.now(); // fetch rtc time
        Serial.printf("Current time: %02d/%02d/%d %02d:%02d:%02d\n", now.month(), now.day(), now.year(), now.hour(), now.minute(), now.second());
    }

    // Check for a new RFID card
    if (rfidOK)
    {
        if (millis() - lastRfidCheck >= rfidPollInterval) // RFID polling must be throttled to avoid starving the microsd card
        {
            lastRfidCheck = millis();
            if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial())
            {
                Serial.print("Card detected! UID: ");

                for (byte i = 0; i < rfid.uid.size; i++)
                {
                    Serial.printf("%02X", rfid.uid.uidByte[i]);

                    if (i < rfid.uid.size - 1)
                        Serial.print(":");
                }
                Serial.println();

                // Stop communicating with this card
                rfid.PICC_HaltA();
                rfid.PCD_StopCrypto1();
            }
        }
    }
}

void listDirectory(fs::FS &fs, const char *dirname, uint8_t levels)
{
    File root = fs.open(dirname);

    if (!root)
    {
        Serial.println("Failed to open directory");
        return;
    }

    if (!root.isDirectory())
    {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();

    while (file)
    {
        if (file.isDirectory())
        {
            Serial.printf("[DIR ] %s\n", file.name());

            if (levels)
            {
                listDirectory(fs, file.path(), levels - 1);
            }
        }
        else
        {
            Serial.printf("[FILE] %-30s %8u bytes\n",
                          file.name(),
                          (unsigned)file.size());
        }

        file = root.openNextFile();
    }
}