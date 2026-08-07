#define USER_SETUP_INFO "custom_clock_user_setup"

// Put this file in the same directory of User_Setup_Select.h and add the following line to the afformentioned file:
//#include <custom_clock_user_setup.h>
// Comment all other user setup include statements.

#define ILI9488_DRIVER     // WARNING: Do not connect ILI9488 display SDO to MISO if other devices share the SPI bus (TFT SDO does NOT tristate when CS is high)

#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 320

#define TFT_MISO  13  // Automatically assigned with ESP8266 if not defined
#define TFT_MOSI  12  // Automatically assigned with ESP8266 if not defined
#define TFT_SCLK  11  // Automatically assigned with ESP8266 if not defined

#define TFT_CS    10  // Chip select control pin
#define TFT_DC    9  // Data Command control pin
#define TFT_RST   8  // Reset pin

#define TFT_BL 7  // LED back-light

#define TOUCH_CS 4     // Chip select pin (T_CS) of touch screen

#define SPI_FREQUENCY  40000000

#define SPI_READ_FREQUENCY  20000000

// The XPT2046 requires a lower SPI clock rate of 2.5MHz so we define that here:
#define SPI_TOUCH_FREQUENCY  2500000

// The ESP32 has 2 free SPI ports i.e. VSPI and HSPI, the VSPI is the default.
// If the VSPI port is in use and pins are not accessible (e.g. TTGO T-Beam)
// then uncomment the following line:
#define USE_HSPI_PORT // NEEDED FOR IT TO WORK

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH