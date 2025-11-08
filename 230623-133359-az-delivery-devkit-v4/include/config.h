#pragma once

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <Preferences.h> // for saving to flash

extern Preferences preferences;
extern WiFiManager wm;

void run_config_portal();

// select which pin will trigger the configuration portal when set to LOW
#define WM_TRIGGER_PIN 4 // blue 4 //silver 27
// ----------------------------------------------------------------

#define USER_ID_LENGTH 7
#define USER_BUFFER_LENGTH 24
#define DATA_LENGTH 16
#define ENCRYPTION_DATA_LENGTH 16

//  https://stackoverflow.com/a/32140193
#define BASE64_USER_ID_LENGTH ((4 * USER_ID_LENGTH / 3) + 3) & ~3
#define BASE64_USER_BUFFER_LENGTH ((4 * USER_BUFFER_LENGTH / 3) + 3) & ~3
#define BASE64_DATA_LENGTH ((4 * DATA_LENGTH / 3) + 3) & ~3
#define BASE64_ENCRYPTION_DATA_LENGTH ((4 * ENCRYPTION_DATA_LENGTH / 3) + 3) & ~3

#define USER_ID_KEY "id"
#define USER_BUFFER_KEY "buf"
#define DATA_KEY "dat"
#define ENCRYPTION_DATA_KEY "enc"

#define BUFFER_LENGTH 255
#define DOCUMENT_LENGTH 256

#define qos 1

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)

#define RST_PIN 9 // Configurable, see typical pin layout above
#define SS_PIN 5  // Configurable, see typical pin layout above

// This is the most important switch: It defines if you want to use Mifare Classic or Desfire EV1 cards.
// If you set this define to false the users will only be identified by the UID of a Mifare Classic or Desfire card.
// This mode is only for testing if you have no Desfire cards available.
// Mifare Classic cards have been cracked due to a badly implemented encryption.
// It is easy to clone a Mifare Classic card (including it's UID).
// You should use Defire EV1 cards for any serious door access system.
// When using Desfire EV1 cards a 16 byte data block is stored in the card's EEPROM memory
// that can only be read with the application master key.
// To clone a Desfire card it would be necessary to crack a 168 bit 3K3DES or a 128 bit AES key data which is impossible.
// If the Desfire card does not contain the correct data the door will not open even if the UID is correct.
// IMPORTANT: After changing this compiler switch, please execute the CLEAR command!
#define USE_DESFIRE true

#if USE_DESFIRE
// This compiler switch defines if you use AES (128 bit) or DES (168 bit) for the PICC master key and the application master key.
// Cryptographers say that AES is better.
// But the disadvantage of AES encryption is that it increases the power consumption of the card more than DES.
// The maximum read distance is 5,3 cm when using 3DES keys and 4,0 cm when using AES keys.
// (When USE_DESFIRE == false the same Desfire card allows a distance of 6,3 cm.)
// If the card is too far away from the antenna you get a timeout error at the moment when the Authenticate command is executed.
// IMPORTANT: Before changing this compiler switch, please execute the RESTORE command on all personalized cards!
#define USE_AES false

// This define should normally be zero
// If you want to run the selftest (only available if USE_DESFIRE == true) you must set this to a value > 0.
// Then you can enter TEST into the terminal to execute a selftest that tests ALL functions in the Desfire class.
// The value that you can specify here is 1 or 2 which will be the debug level for the selftest.
// At level 2 you see additionally the CMAC and the data sent to and received from the card.
#define COMPILE_SELFTEST 0

// This define should normally be false
// If this is true you can use Classic cards / keyfobs additionally to Desfire cards.
// This means that the code is compiled for Defire cards, but when a Classic card is detected it will also work.
// This mode is not recommended because Classic cards do not offer the same security as Desfire cards.
#define ALLOW_ALSO_CLASSIC false
#endif

// This password will be required when entering via Terminal
// If you define an empty string here, no password is requested.
// If any unauthorized person may access the dooropener hardware phyically you should provide a password!
#define PASSWORD ""
// The interval of inactivity in minutes after which the password must be entered again (automatic log-off)
#define PASSWORD_TIMEOUT 5

// This Arduino / Teensy pin is connected to the PN532 RSTPDN pin (reset the PN532)
// When a communication error with the PN532 is detected the board is reset automatically.
#define RESET_PIN 16
// The software SPI SCK  pin (Clock)
#define SPI_CLK_PIN 18
// The software SPI MISO pin (Master In, Slave Out)
#define SPI_MISO_PIN 19
// The software SPI MOSI pin (Master Out, Slave In)
#define SPI_MOSI_PIN 23
// The software SPI SSEL pin (Chip Select)
#define SPI_CS_PIN 27 // 27 for blue 5 for silver

#define learn 17
// The interval in milliseconds that the relay is powered which opens the door
#define OPEN_INTERVAL 100

#define enc_key_length 16

#define NAME_BUF_SIZE 24

// This is the interval that the RF field is switched off to save battery.
// The shorter this interval, the more power is consumed by the PN532.
// The longer  this interval, the longer the user has to wait until the door opens.
// The recommended interval is 1000 ms.
// Please note that the slowness of reading a Desfire card is not caused by this interval.
// The SPI bus speed is throttled to 10 kHz, which allows to transmit the data over a long cable,
// but this obviously makes reading the card slower.
#define RF_OFF_INTERVAL 1000

// #include <base64.h>             //for parsing base64
// #include <ArduinoJson.h>        //for parsing json

// ######################################################################################

// #include "UserManager.h"

// The tick counter starts at zero when the CPU is reset.
// This interval is added to the 64 bit tick count to get a value that does not start at zero,
// because gu64_LastPasswd is initialized with 0 and must always be in the past.
#define PASSWORD_OFFSET_MS (2 * PASSWORD_TIMEOUT * 60 * 1000)
