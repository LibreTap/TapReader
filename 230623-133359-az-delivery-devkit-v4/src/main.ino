/**************************************************************************
    
  @author   Elmü
  DIY electronic RFID Door Lock with Battery Backup (2016)

  Check for a new version of this code on 
  http://www.codeproject.com/Articles/1096861/DIY-electronic-RFID-Door-Lock-with-Battery-Backup

**************************************************************************/



#include <Arduino.h>
#include <ArduinoJson.h>
#include <base64.hpp>
#include <EspMQTTClient.h>

#include <Crypto.h>
#include <AES.h>
#include <string.h>

#include <LiquidCrystal_I2C.h>


#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "bitmap.h"


// ------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <Preferences.h> // for saving to flash
Preferences preferences;

struct {
String url;
String usr;
String pw;
String id;
String SSID;
String KEY;
int port;
} mqtt_data;

 
// select which pin will trigger the configuration portal when set to LOW
#define TRIGGER_PIN 4 //blue 4 //silver 27
WiFiManager wm; 


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

String clientID = "TestClient";

AES128 aes128;



#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define RST_PIN         9          // Configurable, see typical pin layout above
#define SS_PIN          5        // Configurable, see typical pin layout above


// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;

// set LCD address, number of columns and rows
// if you don't know your display address, run an I2C scanner sketch
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  

enum state {
  WAITING_FOR_USER_ID,
  WAITING_FOR_USER_BUFFER,
} current_state;

int last_state_change;

char last_will[200] = {}; // to fix wierd pointer issure with the last will message
const char* last_msg = last_will;


enum mode {
  NONE,
  AUTHENTICATE,
  REGISTER,
} current_mode;

EspMQTTClient client(
  "WLAN-G69TFX",      // Wifi ssid
  "68646696578448615281",  // Wifi password
  "mqtt.olga-tech.de",  // MQTT Broker server ip
  "testuser2",   // Can be omitted if not needed
  "testuser2",   // Can be omitted if not needed
  clientID.c_str(),     // Client name that uniquely identify your device
  1883              // The MQTT port, default to 1883. this line can be omitted
);

// Our configuration structure.
//
// Never use a JsonDocument to store the configuration!
// A JsonDocument is *not* a permanent storage; it's only a temporary storage
// used during the serialization phase. See:
// https://arduinojson.org/v6/faq/why-must-i-create-a-separate-config-object/
struct Message {
  unsigned char user_id[USER_ID_LENGTH + 1];
  unsigned char user_buffer[USER_BUFFER_LENGTH + 1];
  unsigned char data[DATA_LENGTH + 1];                          //for transmitting the key
  unsigned char encryption_data[ENCRYPTION_DATA_LENGTH + 1];    //for getting random data and sending back the encipted
};

Message in_message;                         // <- global configuration object
Message out_message;
unsigned char out_buffer[BUFFER_LENGTH + 1];



// ------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------


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
#define USE_DESFIRE   true

#if USE_DESFIRE
    // This compiler switch defines if you use AES (128 bit) or DES (168 bit) for the PICC master key and the application master key.
    // Cryptographers say that AES is better.
    // But the disadvantage of AES encryption is that it increases the power consumption of the card more than DES.
    // The maximum read distance is 5,3 cm when using 3DES keys and 4,0 cm when using AES keys.
    // (When USE_DESFIRE == false the same Desfire card allows a distance of 6,3 cm.)
    // If the card is too far away from the antenna you get a timeout error at the moment when the Authenticate command is executed.
    // IMPORTANT: Before changing this compiler switch, please execute the RESTORE command on all personalized cards!
    #define USE_AES   false

    // This define should normally be zero
    // If you want to run the selftest (only available if USE_DESFIRE == true) you must set this to a value > 0.
    // Then you can enter TEST into the terminal to execute a selftest that tests ALL functions in the Desfire class.
    // The value that you can specify here is 1 or 2 which will be the debug level for the selftest.
    // At level 2 you see additionally the CMAC and the data sent to and received from the card.
    #define COMPILE_SELFTEST  0
    
    // This define should normally be false
    // If this is true you can use Classic cards / keyfobs additionally to Desfire cards.
    // This means that the code is compiled for Defire cards, but when a Classic card is detected it will also work.
    // This mode is not recommended because Classic cards do not offer the same security as Desfire cards.
    #define ALLOW_ALSO_CLASSIC   false
#endif

// This password will be required when entering via Terminal
// If you define an empty string here, no password is requested.
// If any unauthorized person may access the dooropener hardware phyically you should provide a password!
#define PASSWORD  ""
// The interval of inactivity in minutes after which the password must be entered again (automatic log-off)
#define PASSWORD_TIMEOUT  5

// This Arduino / Teensy pin is connected to the PN532 RSTPDN pin (reset the PN532)
// When a communication error with the PN532 is detected the board is reset automatically.
#define RESET_PIN         16
// The software SPI SCK  pin (Clock)
#define SPI_CLK_PIN       18
// The software SPI MISO pin (Master In, Slave Out)
#define SPI_MISO_PIN      19
// The software SPI MOSI pin (Master Out, Slave In)
#define SPI_MOSI_PIN      23
// The software SPI SSEL pin (Chip Select)
#define SPI_CS_PIN        27 // 27 for blue 5 for silver
 
#define learn             17
// The interval in milliseconds that the relay is powered which opens the door
#define OPEN_INTERVAL   100


#define enc_key_length 16

#define NAME_BUF_SIZE   24

// This is the interval that the RF field is switched off to save battery.
// The shorter this interval, the more power is consumed by the PN532.
// The longer  this interval, the longer the user has to wait until the door opens.
// The recommended interval is 1000 ms.
// Please note that the slowness of reading a Desfire card is not caused by this interval.
// The SPI bus speed is throttled to 10 kHz, which allows to transmit the data over a long cable, 
// but this obviously makes reading the card slower.
#define RF_OFF_INTERVAL  1000

//#include <base64.h>             //for parsing base64
//#include <ArduinoJson.h>        //for parsing json


// ######################################################################################

#if USE_DESFIRE
    #if USE_AES
        #define DESFIRE_KEY_TYPE   AES
        #define DEFAULT_APP_KEY    gi_PN532.AES_DEFAULT_KEY
    #else
        #define DESFIRE_KEY_TYPE   DES
        #define DEFAULT_APP_KEY    gi_PN532.DES3_DEFAULT_KEY
    #endif
    
    #include "Desfire.h"
    #include "Secrets.h"
    #include "Buffer.h"
    Desfire          gi_PN532; // The class instance that communicates with Mifare Desfire cards   
    DESFIRE_KEY_TYPE gi_PiccMasterKey;
#else
    #include "Classic.h"
    Classic          gi_PN532; // The class instance that communicates with Mifare Classic cards
#endif

//#include "UserManager.h"

// The tick counter starts at zero when the CPU is reset.
// This interval is added to the 64 bit tick count to get a value that does not start at zero,
// because gu64_LastPasswd is initialized with 0 and must always be in the past.
#define PASSWORD_OFFSET_MS   (2 * PASSWORD_TIMEOUT * 60 * 1000)


struct kCard
{
    byte     u8_UidLength;   // UID = 4 or 7 bytes
    byte     u8_KeyVersion;  // for Desfire random ID cards
    bool      b_PN532_Error; // true -> the error comes from the PN532, false -> crypto error
    eCardType e_CardType;    
};


// user structure
struct kUser
{
    // Constructor
    kUser()
    {
        memset(this, 0, sizeof(kUser));
    }

    // Card ID (4 or 7 bytes), binary
    union 
    {
        uint64_t  u64;      
        byte      u8[8];
    } ID;
   
    // User name (plain text + terminating zero character) + appended random data if name is shorter than NAME_BUF_SIZE
    char s8_Name[NAME_BUF_SIZE]; 

};




// global variables
char       gs8_CommandBuffer[500];    // Stores commands typed by the user via Terminal and the password
uint32_t   gu32_CommandPos   = 0;     // Index in gs8_CommandBuffer
uint64_t   gu64_LastPasswd   = 0;     // Timestamp when the user has enetered the password successfully
uint64_t   gu64_LastID       = 0;     // The last card UID that has been read by the RFID reader  
bool       gb_InitSuccess    = false; // true if the PN532 has been initialized successfully

kCard last_card;

unsigned char id_vault[7] = {0};

void setup() 
{   // Open USB serial port
    Serial.begin(115200);

    // initialize LCD
    lcd.init();
    // turn on LCD backlight                      
    lcd.backlight();

    display_clear();

    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.display();

    pinMode(TRIGGER_PIN, INPUT_PULLUP);
    Serial.println(wm.getWiFiSSID());
    Serial.println(wm.getWiFiPass());
    
    preferences.begin("my-app", false);
    
    if ( digitalRead(TRIGGER_PIN) == LOW) {
        WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP 
        display_settings_mode(); 
        run_config_portal();
    }
    load_flash();
    preferences.end();
    
    client.setWifiCredentials(mqtt_data.SSID.c_str(),mqtt_data.KEY.c_str());
    client.setMqttServer(mqtt_data.url.c_str(),mqtt_data.usr.c_str(),mqtt_data.pw.c_str(),mqtt_data.port);
    client.setMqttClientName(mqtt_data.id.c_str());

    // Software SPI is configured to run a slow clock of 10 kHz which can be transmitted over longer cables.
    gi_PN532.InitSoftwareSPI(SPI_CLK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SPI_CS_PIN, RESET_PIN);

    pinMode(learn,INPUT_PULLUP);


    

    InitReader(false);

    #if USE_DESFIRE
        gi_PiccMasterKey.SetKeyData(SECRET_PICC_MASTER_KEY, sizeof(SECRET_PICC_MASTER_KEY), CARD_KEY_VERSION);
    #endif

    // -------------------------------------------------------------------------------------------------------
    // -------------------------------------------------------------------------------------------------------

    current_state = WAITING_FOR_USER_ID;
    last_state_change = millis();
    current_mode = NONE;

    client.setMaxPacketSize(500);
    // Optional functionalities of EspMQTTClient
    client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
    //client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
    // client.enableOTA(); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
    client.enableDrasticResetOnConnectionFailures();
    
    /*char buffer[100]; // Make sure the buffer is large enough
    char concatenatedString[100]; // Make sure this buffer is large enough too

    char disconnect[20];
    strcpy(disconnect,"disconnect");


    strcpy(buffer, "device/");
    strcat(buffer, clientID.c_str());
    strcat(buffer, "/register");

    strcpy(concatenatedString, buffer); // Copy the concatenated string*/

    String message3 = "device/" + String(clientID) + "/register";
    message3.toCharArray(last_will,message3.length()+1);
    client.enableLastWillMessage(last_msg, "disconnect", true);  // You can activate the retain flag by setting the third parameter to true
    
    
    // -------------------------------------------------------------------------------------------------------
    // -------------------------------------------------------------------------------------------------------
}



    // -------------------------------------------------------------------------------------------------------
    // -------------------------------------------------------------------------------------------------------

    
    // -------------------------------------------------------------------------------------------------------
    // -------------------------------------------------------------------------------------------------------



void loop()
{   
    client.loop();

    if(!(client.isMqttConnected() && client.isWifiConnected())){
       display_connectionloss(); 
       return;
    }

    

    // Handle waiting_for_user_buffer timeout
    if (current_state == WAITING_FOR_USER_BUFFER && millis() - last_state_change > 20000) {
        Serial.println("Timeout while waiting for user buffer");
        current_state = WAITING_FOR_USER_ID;
        display_fail();
        delay(1000);
    }

    if (current_mode == NONE) {
        display_mode_standby();
        return;
    }

    
    
    if(current_state == WAITING_FOR_USER_ID && current_mode == AUTHENTICATE){
    
    display_karte_auflegen();
    
    unsigned char ID[8] = {0};

    clear_kCard(&last_card);
    if (ReadCard(ID, &last_card)){
        // No card present in the RF field
        if (last_card.u8_UidLength > 0) 
        {
            clearMessage(out_message);
            memcpy(out_message.user_id, ID, USER_ID_LENGTH+1);    
            serializeMessage(out_buffer, out_message);
            client.publish("device/" + clientID + "/data", (char*)out_buffer);
            current_state = WAITING_FOR_USER_BUFFER;
            printUnsignedCharArrayAsHex(ID,8);
            last_state_change = millis();
            display_processing();
        }

        else{
            gu64_LastID = 0;
        }

        
    }

    else{
        
        if (IsDesfireTimeout())
            {
                display_fail();
                delay(1000);
                display_karte_auflegen();
                // Nothing to do here because IsDesfireTimeout() prints additional error message and blinks the red LED
            }
            else if (last_card.b_PN532_Error) // Another error from PN532 -> reset the chip
            {  
                display_fail();
                delay(1000);
                display_karte_auflegen();
                InitReader(true); // flash red LED for 2.4 seconds
            }
            else // e.g. Error while authenticating with master key
            {
                display_fail();
                delay(1000);
                display_karte_auflegen();
                //FlashLED(LED_RED, 1000);
            }
            
            Utils::Print("> ");       

    }

    }

}





// Reset the PN532 chip and initialize, set gb_InitSuccess = true on success
// If b_ShowError == true -> flash the red LED very slowly
void InitReader(bool b_ShowError)
{
    if (b_ShowError)
    {
        //SetLED(LED_RED);
        Utils::Print("Communication Error -> Reset PN532\r\n");
    }

    do // pseudo loop (just used for aborting with break;)
    {
        gb_InitSuccess = false;
      
        // Reset the PN532
        gi_PN532.begin(); // delay > 400 ms
    
        byte IC, VersionHi, VersionLo, Flags;
        if (!gi_PN532.GetFirmwareVersion(&IC, &VersionHi, &VersionLo, &Flags))
            break;
    
        char Buf[80];
        sprintf(Buf, "Chip: PN5%02X, Firmware version: %d.%d\r\n", IC, VersionHi, VersionLo);
        Utils::Print(Buf);
        sprintf(Buf, "Supports ISO 14443A:%s, ISO 14443B:%s, ISO 18092:%s\r\n", (Flags & 1) ? "Yes" : "No",
                                                                                (Flags & 2) ? "Yes" : "No",
                                                                                (Flags & 4) ? "Yes" : "No");
        Utils::Print(Buf);
         
        // Set the max number of retry attempts to read from a card.
        // This prevents us from waiting forever for a card, which is the default behaviour of the PN532.
        if (!gi_PN532.SetPassiveActivationRetries())
            break;
        
        // configure the PN532 to read RFID tagscustomize_card
        if (!gi_PN532.SamConfig())
            break;
    
        gb_InitSuccess = true;
    }
    while (false);

    if (b_ShowError)
    {
        Utils::DelayMilli(2000); // a long interval to make the LED flash very slowly        
        //SetLED(LED_OFF);
        Utils::DelayMilli(100);
    }  
}


// ================================================================================

// Modifing for sending to server
// you can call this after the server odered authenication mode
// Stores a new user and his card in the EEPROM of the Teensy
// formerly known as AddCardToEeprom
//  you get the user buffer along with the encription key from the server.
bool customize_card(const char* user_buff, const unsigned char* encript_key, unsigned char* ID)
{   
    display_karte_auflegen();
    kUser k_User;
    kCard k_Card;   
    if (!WaitForCard(&k_User, &k_Card))
        return false;
     
    // First the entire memory of s8_Name is filled with random data.
    // Then the username + terminating zero is written over it.
    // The result is for example: s8_Name[NAME_BUF_SIZE] = { 'P', 'e', 't', 'e', 'r', 0, 0xDE, 0x45, 0x70, 0x5A, 0xF9, 0x11, 0xAB }
    // The string operations like stricmp() will only read up to the terminating zero, 
    // but the application master key is derived from user name + random data.
    //Utils::GenerateRandom((byte*)k_User.s8_Name, NAME_BUF_SIZE);
    // fill the name field with originally used for the name with the user buffer data, that is used to generate key and store value
    display_processing();
    strcpy(k_User.s8_Name, user_buff);
  
    #if USE_DESFIRE
        if ((k_Card.e_CardType & CARD_Desfire) == 0) // Classic
        {
            #if !ALLOW_ALSO_CLASSIC
                Utils::Print("The card is not a Desfire card.\r\n");
                return false;
            #endif
        }
        else // Desfire
        {    
            if (!ChangePiccMasterKey())
                return false;

            if (k_Card.e_CardType != CARD_DesRandom)
            {
                // The secret stored in a file on the card is not required when using a card with random ID 
                // because obtaining the real card UID already requires the PICC master key. This is enough security.
                if (!StoreDesfireSecret(&k_User, encript_key))
                {
                    Utils::Print("Could not personalize the card.\r\n");
                    return false;
                }
            }
        }
    #endif

    // By default a new user can open door one
    //k_User.u8_Flags = DOOR_ONE;

    //vault = k_User; // hab ich eingebaut und bin nicht stolz

   
    memcpy(ID, k_User.ID.u8, 7);
    Utils::Print("Customisatzion done");
    return true;
    //UserManager::StoreNewUser(&k_User);
}


// Waits for the user to approximate the card to the reader
// Timeout = 30 seconds
// Fills in pk_Card competely, but writes only the UID to pk_User.
bool WaitForCard(kUser* pk_User, kCard* pk_Card)
{
    Utils::Print("Please approximate the card to the reader now!\r\nYou have 30 seconds. Abort with ESC.\r\n");
    uint64_t u64_Start = Utils::GetMillis64();
    
    while (true)
    {
        if (ReadCard(pk_User->ID.u8, pk_Card) && pk_Card->u8_UidLength > 0)
        {
            // Avoid that later the door is opened for this card if the card is a long time in the RF field.
            gu64_LastID = pk_User->ID.u64;

            // All the stuff in this function takes about 2 seconds because the SPI bus speed has been throttled to 10 kHz.
            Utils::Print("Processing... (please do not remove the card)\r\n");
            return true;
        }
      
        if ((Utils::GetMillis64() - u64_Start) > 30000)
        {
            Utils::Print("Timeout waiting for card.\r\n");
            return false;
        }

        if (SerialClass::Read() == 27) // ESCAPE
        {
            Utils::Print("Aborted.\r\n");
            return false;
        }
    }
}


//with this card parameter and uid can be extracted and after this send to the server
// Reads the card in the RF field.
// In case of a Random ID card reads the real UID of the card (requires PICC authentication)
// ATTENTION: If no card is present, this function returns true. This is not an error. (check that pk_Card->u8_UidLength > 0)
// pk_Card->u8_KeyVersion is > 0 if a random ID card did a valid authentication with SECRET_PICC_MASTER_KEY
// pk_Card->b_PN532_Error is set true if the error comes from the PN532.
bool ReadCard(byte u8_UID[8], kCard* pk_Card)
{
    memset(pk_Card, 0, sizeof(kCard));
  
    if (!gi_PN532.ReadPassiveTargetID(u8_UID, &pk_Card->u8_UidLength, &pk_Card->e_CardType))
    {
        pk_Card->b_PN532_Error = true;
        return false;
    }

    if (pk_Card->e_CardType == CARD_DesRandom) // The card is a Desfire card in random ID mode
    {
        #if USE_DESFIRE
            if (!AuthenticatePICC(&pk_Card->u8_KeyVersion))
                return false;
        
            // replace the random ID with the real UID
            if (!gi_PN532.GetRealCardID(u8_UID))
                return false;

            pk_Card->u8_UidLength = 7; // random ID is only 4 bytes
        #else
            Utils::Print("Cards with random ID are not supported in Classic mode.\r\n");
            return false;    
        #endif
    }
    return true;
}

// returns true if the cause of the last error was a Timeout.
// This may happen for Desfire cards when the card is too far away from the reader.
bool IsDesfireTimeout()
{
    #if USE_DESFIRE
        // For more details about this error see comment of GetLastPN532Error()
        if (gi_PN532.GetLastPN532Error() == 0x01) // Timeout
        {
            Utils::Print("A Timeout mostly means that the card is too far away from the reader.\r\n");
            
            // In this special case we make a short pause only because someone tries to open the door 
            // -> don't let him wait unnecessarily.
            //FlashLED(LED_RED, 200);
            return true;
        }
    #endif
    return false;
}


// b_PiccAuth = true if random ID card with successful authentication with SECRET_PICC_MASTER_KEY
bool authenticate_user(unsigned char* ID, char* user_buffer, kCard* pk_Card, unsigned char* key_ret)
{
    kUser k_User;  
    
    memcpy(k_User.ID.u8, ID, 7);
    memcpy(k_User.s8_Name, user_buffer,NAME_BUF_SIZE);

    /*if (!(u64_ID == vault.ID.u64))
    {
        Utils::Print("Unknown person tries to open the door: ");
        Utils::PrintHexBuf((byte*)&u64_ID, 7, LF);
        //FlashLED(LED_RED, 1000);
        return;
    }*/

    //copyUser(vault,k_User);

    #if USE_DESFIRE
        if ((pk_Card->e_CardType & CARD_Desfire) == 0) // Classic
        {
            #if !ALLOW_ALSO_CLASSIC
                Utils::Print("The card is not a Desfire card.\r\n");
                //FlashLED(LED_RED, 1000);
                return false;
            #endif
        }
        else // Desfire
        {
            if (pk_Card->e_CardType == CARD_DesRandom) // random ID Desfire card
            {
                // In case of a random ID card the authentication has already been done in ReadCard().
                // But ReadCard() may also authenticate with the factory default DES key, so we must check here 
                // that SECRET_PICC_MASTER_KEY has been used for authentication.
                if (pk_Card->u8_KeyVersion != CARD_KEY_VERSION)
                {
                    Utils::Print("The card is not personalized.\r\n");
                    //FlashLED(LED_RED, 1000);
                    return false;
                }
            }
            else // default Desfire card
            {   
                ///unsigned char key[enc_key_length] = {0};
                if (!CheckDesfireSecret(&k_User,key_ret))
                {
                    if (IsDesfireTimeout()) // Prints additional error message and blinks the red LED
                        return false;
        
                    Utils::Print("The card is not personalized.\r\n");
                    //Utils::Print("Hier ist das Problem");
                    //FlashLED(LED_RED, 1000);
                    return false;
                }
            }
        }
    #endif


    Utils::Print("Authentic");
    //Utils::Print(k_User.s8_Name);
    switch (pk_Card->e_CardType)
    {
        case CARD_DesRandom: Utils::Print(" (Desfire random card)",  LF); break;
        case CARD_Desfire:   Utils::Print(" (Desfire default card)", LF); break;
        default:             Utils::Print(" (Classic card)",         LF); break;
    }

    //ActivateRelais(k_User.u8_Flags);

    // Avoid that the door is opened twice when the card is in the RF field for a longer time.
    gu64_LastID = k_User.ID.u64;
    return true;

}



// =================================== DESFIRE ONLY =========================================

#if USE_DESFIRE

// If the card is personalized -> authenticate with SECRET_PICC_MASTER_KEY,
// otherwise authenticate with the factory default DES key.
bool AuthenticatePICC(byte* pu8_KeyVersion)
{
    if (!gi_PN532.SelectApplication(0x000000)) // PICC level
        return false;

    if (!gi_PN532.GetKeyVersion(0, pu8_KeyVersion)) // Get version of PICC master key
        return false;

    // The factory default key has version 0, while a personalized card has key version CARD_KEY_VERSION
    if (*pu8_KeyVersion == CARD_KEY_VERSION)
    {
        if (!gi_PN532.Authenticate(0, &gi_PiccMasterKey))
            return false;
    }
    else // The card is still in factory default state
    {
        if (!gi_PN532.Authenticate(0, &gi_PN532.DES2_DEFAULT_KEY))
            return false;
    }
    return true;
}

// Generate two dynamic secrets: the Application master key (AES 16 byte or DES 24 byte) and the 16 byte StoreValue.
// Both are derived from the 7 byte card UID and the the user name + random data stored in EEPROM using two 24 byte 3K3DES keys.
// This function takes only 6 milliseconds to do the cryptographic calculations.
bool GenerateDesfireSecrets(kUser* pk_User, DESFireKey* pi_AppMasterKey, byte u8_StoreValue[16])
{
    // The buffer is initialized to zero here
    byte u8_Data[24] = {0}; 

    // Copy the 7 byte card UID into the buffer
    memcpy(u8_Data, pk_User->ID.u8, 7);

    // XOR the user name and the random data that are stored in EEPROM over the buffer.
    // s8_Name[NAME_BUF_SIZE] contains for example { 'P', 'e', 't', 'e', 'r', 0, 0xDE, 0x45, 0x70, 0x5A, 0xF9, 0x11, 0xAB }
    int B=0;
    for (int N=0; N<NAME_BUF_SIZE; N++)
    {
        u8_Data[B++] ^= pk_User->s8_Name[N];
        if (B > 15) B = 0; // Fill the first 16 bytes of u8_Data, the rest remains zero.
    }

    byte u8_AppMasterKey[24];

    DES i_3KDes;
    if (!i_3KDes.SetKeyData(SECRET_APPLICATION_KEY, sizeof(SECRET_APPLICATION_KEY), 0) || // set a 24 byte key (168 bit)
        !i_3KDes.CryptDataCBC(CBC_SEND, KEY_ENCIPHER, u8_AppMasterKey, u8_Data, 24))
        return false;
    
    if (!i_3KDes.SetKeyData(SECRET_STORE_VALUE_KEY, sizeof(SECRET_STORE_VALUE_KEY), 0) || // set a 24 byte key (168 bit)
        !i_3KDes.CryptDataCBC(CBC_SEND, KEY_ENCIPHER, u8_StoreValue, u8_Data, 16))
        return false;

    // If the key is an AES key only the first 16 bytes will be used
    if (!pi_AppMasterKey->SetKeyData(u8_AppMasterKey, sizeof(u8_AppMasterKey), CARD_KEY_VERSION))
        return false;

    return true;
}

// Check that the data stored on the card is the same as the secret generated by GenerateDesfireSecrets()
// get the enctiption key from the card
bool CheckDesfireSecret(kUser* pk_User, unsigned char* enc_key)
{
    DESFIRE_KEY_TYPE i_AppMasterKey;
    byte u8_StoreValue[16];
    if (!GenerateDesfireSecrets(pk_User, &i_AppMasterKey, u8_StoreValue))
        return false;

    if (!gi_PN532.SelectApplication(0x000000)) // PICC level
        return false;

    byte u8_Version; 
    if (!gi_PN532.GetKeyVersion(0, &u8_Version))
        return false;

    // The factory default key has version 0, while a personalized card has key version CARD_KEY_VERSION
    if (u8_Version != CARD_KEY_VERSION)
        return false;

    if (!gi_PN532.SelectApplication(CARD_APPLICATION_ID))
        return false;

    if (!gi_PN532.Authenticate(0, &i_AppMasterKey))
        return false;

    // Read the 16 byte secret from the card
    byte u8_FileData[16];
    if (!gi_PN532.ReadFileData(CARD_FILE_ID, 0, 16, u8_FileData))
        return false;

    if (memcmp(u8_FileData, u8_StoreValue, 16) != 0)
        return false;

    //reading the encription key from the card and putting it into the return variable
    if (!gi_PN532.ReadFileData(CARD_FILE_ID, 16, enc_key_length, enc_key))
        return false;

    return true;
}

// Store the SECRET_PICC_MASTER_KEY on the card
bool ChangePiccMasterKey()
{
    byte u8_KeyVersion;
    if (!AuthenticatePICC(&u8_KeyVersion))
        return false;

    if (u8_KeyVersion != CARD_KEY_VERSION) // empty card
    {
        // Store the secret PICC master key on the card.
        if (!gi_PN532.ChangeKey(0, &gi_PiccMasterKey, NULL))
            return false;

        // A key change always requires a new authentication
        if (!gi_PN532.Authenticate(0, &gi_PiccMasterKey))
            return false;
    }
    return true;
}

// Create the application SECRET_APPLICATION_ID,
// store the dynamic Application master key in the application,
// create a StandardDataFile SECRET_FILE_ID and store the dynamic 16 byte value into that file.
// This function requires previous authentication with PICC master key.
bool StoreDesfireSecret(kUser* pk_User, const unsigned char* enc_key)
{
    if (CARD_APPLICATION_ID == 0x000000 || CARD_KEY_VERSION == 0)
        return false; // severe errors in Secrets.h -> abort
  
    DESFIRE_KEY_TYPE i_AppMasterKey;
    byte u8_StoreValue[16];
    if (!GenerateDesfireSecrets(pk_User, &i_AppMasterKey, u8_StoreValue))
        return false;

    // First delete the application (The current application master key may have changed after changing the user name for that card)
    if (!gi_PN532.DeleteApplicationIfExists(CARD_APPLICATION_ID))
        return false;

    // Create the new application with default settings (we must still have permission to change the application master key later)
    if (!gi_PN532.CreateApplication(CARD_APPLICATION_ID, KS_FACTORY_DEFAULT, 1, i_AppMasterKey.GetKeyType()))
        return false;

    // After this command all the following commands will apply to the application (rather than the PICC)
    if (!gi_PN532.SelectApplication(CARD_APPLICATION_ID))
        return false;

    // Authentication with the application's master key is required
    if (!gi_PN532.Authenticate(0, &DEFAULT_APP_KEY))
        return false;

    // Change the master key of the application
    if (!gi_PN532.ChangeKey(0, &i_AppMasterKey, NULL))
        return false;

    // A key change always requires a new authentication with the new key
    if (!gi_PN532.Authenticate(0, &i_AppMasterKey))
        return false;

    // After this command the application's master key and it's settings will be frozen. They cannot be changed anymore.
    // To read or enumerate any content (files) in the application the application master key will be required.
    // Even if someone knows the PICC master key, he will neither be able to read the data in this application nor to change the app master key.
    if (!gi_PN532.ChangeKeySettings(KS_CHANGE_KEY_FROZEN))
        return false;

    // --------------------------------------------

    // Create Standard Data File with 16 bytes length
    DESFireFilePermissions k_Permis;
    k_Permis.e_ReadAccess         = AR_KEY0;
    k_Permis.e_WriteAccess        = AR_KEY0;
    k_Permis.e_ReadAndWriteAccess = AR_KEY0;
    k_Permis.e_ChangeAccess       = AR_KEY0;
    if (!gi_PN532.CreateStdDataFile(CARD_FILE_ID, &k_Permis, (16+enc_key_length)))
        return false;

    // Write the StoreValue into that file
    if (!gi_PN532.WriteFileData(CARD_FILE_ID, 0, 16, u8_StoreValue))
        return false;       
    
    // Write the StoreValue into that file
    if (!gi_PN532.WriteFileData(CARD_FILE_ID, 16, enc_key_length, enc_key))
        return false;  

    return true;
}

// If you have already written the master key to a card and want to use the card for another purpose 
// you can restore the master key with this function. Additionally the application SECRET_APPLICATION_ID is deleted.
// If a user has been stored in the EEPROM for this card he will also be deleted.
bool RestoreDesfireCard()
{
    kUser k_User;
    kCard k_Card;  
    if (!WaitForCard(&k_User, &k_Card))
        return false;

    //UserManager::DeleteUser(k_User.ID.u64, NULL);    

    if ((k_Card.e_CardType & CARD_Desfire) == 0)
    {
        Utils::Print("The card is not a Desfire card.\r\n");
        return false;
    }

    byte u8_KeyVersion;
    if (!AuthenticatePICC(&u8_KeyVersion))
        return false;

    // If the key version is zero AuthenticatePICC() has already successfully authenticated with the factory default DES key
    if (u8_KeyVersion == 0)
        return true;

    // An error in DeleteApplication must not abort. 
    // The key change below is more important and must always be executed.
    bool b_Success = gi_PN532.DeleteApplicationIfExists(CARD_APPLICATION_ID);
    if (!b_Success)
    {
        // After any error the card demands a new authentication
        if (!gi_PN532.Authenticate(0, &gi_PiccMasterKey))
            return false;
    }
    
    if (!gi_PN532.ChangeKey(0, &gi_PN532.DES2_DEFAULT_KEY, NULL))
        return false;

    // Check if the key change was successfull
    if (!gi_PN532.Authenticate(0, &gi_PN532.DES2_DEFAULT_KEY))
        return false;

    return b_Success;
}

/*
bool MakeRandomCard()
{
    Utils::Print("\r\nATTENTION: Configuring the card to send a random ID cannot be reversed.\r\nThe card will be a random ID card FOREVER!\r\nIf you are really sure what you are doing hit 'Y' otherwise hit 'N'.\r\n\r\n");
    if (!WaitForKeyYesNo())
        return false;
    
    kUser k_User;
    kCard k_Card;  
    if (!WaitForCard(&k_User, &k_Card))
        return false;

    if ((k_Card.e_CardType & CARD_Desfire) == 0)
    {
        Utils::Print("The card is not a Desfire card.\r\n");
        return false;
    }

    byte u8_KeyVersion;
    if (!AuthenticatePICC(&u8_KeyVersion))
        return false;

    return gi_PN532.EnableRandomIDForever();
}
*/

 static void copyUser(const kUser& source, kUser& destination)
    {
    destination.ID.u64 = source.ID.u64;
    memcpy(destination.s8_Name, source.s8_Name, NAME_BUF_SIZE);
    }


#endif // USE_DESFIRE



// ----------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------------------


void clearBuffer(unsigned char *buffer, size_t size) {
  for (int i = 0; i < size; i++) {
    buffer[i] = '\0';
  }
}

void clearMessage(Message &message) {
  clearBuffer(message.user_id, USER_ID_LENGTH);
  clearBuffer(message.user_buffer, USER_BUFFER_LENGTH);
  clearBuffer(message.data, DATA_LENGTH);
  clearBuffer(message.encryption_data, ENCRYPTION_DATA_LENGTH);
}

void encode(const unsigned char *buffer, size_t size, unsigned char *out_buffer) {
  // encode_base64() places a null terminator automatically, because the output is a string
  int length = 0;
  while (length < size && buffer[length] != '\0') {
    length++;
  }
  encode_base64(buffer, length, out_buffer);
}

void decode(const char *buffer, size_t buffer_size, unsigned char *out_buffer) {
  unsigned char copy_buffer[buffer_size + 1];
  strlcpy((char*)copy_buffer, buffer, buffer_size + 1);
  // decode_base64() does not place a null terminator, because the output is not always a string
  unsigned int string_length = decode_base64(copy_buffer, buffer_size, out_buffer);
  out_buffer[string_length] = '\0';
}

void serializeMessage(unsigned char *buffer, const Message &message) {
  clearBuffer(buffer, BUFFER_LENGTH);
  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use https://arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<DOCUMENT_LENGTH> doc;

  unsigned char user_id[BASE64_USER_ID_LENGTH];
  unsigned char user_buffer[BASE64_USER_BUFFER_LENGTH];
  unsigned char data[BASE64_DATA_LENGTH];
  unsigned char encryption_data[BASE64_ENCRYPTION_DATA_LENGTH];

  encode(message.user_id, USER_ID_LENGTH, user_id);
  encode(message.user_buffer, USER_BUFFER_LENGTH, user_buffer);
  encode(message.data, DATA_LENGTH, data);
  encode(message.encryption_data, ENCRYPTION_DATA_LENGTH, encryption_data);

  // Set the values in the document
  doc[USER_ID_KEY] = user_id;
  doc[USER_BUFFER_KEY] = user_buffer;
  doc[DATA_KEY] = data;                             //for transmitting the key
  doc[ENCRYPTION_DATA_KEY] = encryption_data;       //for getting random data and sending back the encipted

  // Serialize JSON to serial
  if (serializeJson(doc, Serial) == 0) {
    Serial.println(F("Failed to serialize message"));
  }

  // Serialize JSON to buffer
  if (serializeJson(doc, buffer, BUFFER_LENGTH) == 0) {
    Serial.println(F("Failed to serialize message"));
  }
}

void deserializeMessage(char *buffer, Message &message) {
  clearMessage(message);

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use https://arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<DOCUMENT_LENGTH> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, buffer, BUFFER_LENGTH);
  if (error) {
    Serial.println(F("Failed to read message"));
    return;
  }

  decode(doc[USER_ID_KEY], BASE64_USER_ID_LENGTH, message.user_id);
  decode(doc[USER_BUFFER_KEY], BASE64_USER_BUFFER_LENGTH, message.user_buffer);
  decode(doc[DATA_KEY], BASE64_DATA_LENGTH, message.data);
  decode(doc[ENCRYPTION_DATA_KEY], BASE64_ENCRYPTION_DATA_LENGTH, message.encryption_data);
}

void deserializeMessage_for_disp(char *buffer, char* line1, char* line2){
    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use https://arduinojson.org/assistant to compute the capacity.
    StaticJsonDocument<DOCUMENT_LENGTH> doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, buffer, BUFFER_LENGTH);
    if (error) {
        Serial.println(F("Failed to read message"));
        return;
    }

    // Get the values from the JSON document and copy them to the provided arrays
    strncpy(line1, doc["line1"], 16);
    strncpy(line2, doc["line2"], 16);

    line1[16] = '\0';
    line2[16] = '\0';
    
}


void handleCommand(const String & command) {
  Serial.println("Received command: " + command);
  if (command == "authenticate") {
    Serial.println("Enable Authenticate Mode");
    display_authenticate_mode();
    current_mode = AUTHENTICATE;
  } else if (command == "register") {
    Serial.println("Enable Register Mode");
    display_register_mode();
    current_mode = REGISTER;
  } else if (command == "idle") {
    Serial.println("Disable Mode");
    display_mode_standby();
    current_mode = NONE;
  } else if (command == "reset") {
    Serial.println("Resetting");
    ESP.restart();
  } else {
    Serial.println("Unknown command");
  }
}

void handleRDisplay(const String & payload){
    // Make a copy of the payload string
    char buffer[payload.length() + 1];
    
    payload.toCharArray(buffer, sizeof(buffer));
    Serial.println(buffer);
    char line1[17] = {};
    char line2[17] = {};

    deserializeMessage_for_disp(buffer, line1, line2);
    
    Serial.println(line1);
    Serial.println(line2);

    if(containsOnlyZeroes(line1)&&containsOnlyZeroes(line2)){
        display_clear();
    }
    else{
        display_lines(line1, line2);
    }
}

void handleData(const String & payload) {
  // Make a copy of the payload string
  char buffer[payload.length() + 1];
  payload.toCharArray(buffer, sizeof(buffer));

  deserializeMessage(buffer, in_message);

  switch (current_mode) {
    case NONE:
      Serial.println("No Mode");
      break;
    case AUTHENTICATE:

        Serial.println("Authenticate Mode");
        // do authentication stuff


        if(current_state == WAITING_FOR_USER_BUFFER){
        unsigned char key[enc_key_length] = {0};
        clearMessage(out_message);

        if(authenticate_user(in_message.user_id,reinterpret_cast<char*>(in_message.user_buffer),&last_card,key)){
            aes128.setKey(key,enc_key_length);
            unsigned char encr_data[16] = {0};
            aes128.encryptBlock(encr_data,in_message.encryption_data);
            memcpy(out_message.user_id, in_message.user_id, USER_ID_LENGTH+1);
            memcpy(out_message.encryption_data, encr_data, ENCRYPTION_DATA_LENGTH);
            serializeMessage(out_buffer, out_message);
            client.publish("device/" + clientID + "/data", (char*)out_buffer);
            display_succsess();
            delay(1000);
        }

        else{
            memcpy(out_message.user_id, in_message.user_id, USER_ID_LENGTH+1);
            memcpy(out_message.encryption_data, "Failed_to_authe", ENCRYPTION_DATA_LENGTH);
            serializeMessage(out_buffer, out_message);
            client.publish("device/" + clientID + "/data", (char*)out_buffer);
            display_fail();
            delay(1000);
        }
        
        clear_kCard(&last_card);

        current_state = WAITING_FOR_USER_ID;
        current_mode = NONE;

        }

        break;

    case REGISTER:
      Serial.println("Register Mode");
      // do register stuff
      clearMessage(out_message);
      if(current_mode == REGISTER){
        if(customize_card(reinterpret_cast<char*>(in_message.user_buffer),in_message.data,out_message.user_id)){
        printUnsignedCharArrayAsHex(in_message.data,16);
        printUnsignedCharArrayAsHex(in_message.encryption_data,16);
        aes128.setKey(in_message.data,enc_key_length);
        unsigned char encr_data[16] = {0};
        aes128.encryptBlock(encr_data,in_message.encryption_data);
        printUnsignedCharArrayAsHex(encr_data,16);
        memcpy(out_message.encryption_data, encr_data, ENCRYPTION_DATA_LENGTH);
        serializeMessage(out_buffer, out_message);
        client.publish("device/" + clientID + "/data", (char*)out_buffer);
        display_succsess();
        delay(1000);
        current_state = WAITING_FOR_USER_ID;
        current_mode = NONE;}
      }
      else{
        //Failed to assign card
        memcpy(out_message.user_id, in_message.user_id, USER_ID_LENGTH+1);
        memcpy(out_message.encryption_data, "Failed_to_assig", ENCRYPTION_DATA_LENGTH);
        serializeMessage(out_buffer, out_message);
        client.publish("device/" + clientID + "/data", (char*)out_buffer);
        display_fail();
        delay(1000);
        current_state = WAITING_FOR_USER_ID;
        current_mode = NONE;
      }
      break;
    default:
      Serial.println("Unknown mode");
  }
}

void onConnectionEstablished()
    {
    // Subscribe to "device/<client>/receive/data" and display received message to Serial
    client.subscribe("device/" + clientID + "/receive/data", [](const String & payload) {
        handleData(payload);
    },qos);

    // Subscribe to "device/<client>/receive/command" and handle command
    client.subscribe("device/" + clientID + "/receive/command", [](const String & payload) {
        handleCommand(payload);
    },qos);

    // Subscribe to "device/<client>/receive/display" and display received message to Serial
    client.subscribe("device/" + clientID + "/receive/display", [](const String & payload) {
        Serial.println("Remote Display Command");
        handleRDisplay(payload);
    },qos);

    // Publish a message to "device/<client>/register"
    client.publish("device/" + clientID + "/register", "connect",true); // You can activate the retain flag by setting the third parameter to true
    }

void clear_kUser(kUser& user)
{
    memset(&user, 0, sizeof(kUser));
}

void clear_kCard(kCard *card) {
    card->u8_UidLength = 0;
    card->u8_KeyVersion = 0;
    card->b_PN532_Error = false;
    card->e_CardType = CARD_Unknown; // assuming 0 is a valid value for eCardType
}

void printUnsignedCharArrayAsHex(const unsigned char* arr, size_t size) {
  for (size_t i = 0; i < size; i++) {
    // Print each element of the array as its hexadecimal representation
    Serial.print(arr[i], HEX);

    // Add a separator between elements (you can change this to your preference)
    Serial.print(" ");
  }

  // Move to a new line after printing the array
  Serial.println();
}

void display_lines(String line1, String line2) {
  lcd.clear();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(line1); // Cast the unsigned char array to a const char* for printing
  lcd.setCursor(0, 1);
  lcd.print(line2); // Cast the unsigned char array to a const char* for printing
}

void display_clear(){
  lcd.clear();
  lcd.noBacklight();
}

void display_karte_auflegen(){
  display.clearDisplay();
  display.drawBitmap(0, 0, karte_auflegen, 128, 64, WHITE);
  display.display();
}

void display_processing(){
  display.clearDisplay();
  display.drawBitmap(0, 0, processing, 128, 64, WHITE);
  display.display();
}

void display_fail(){
  display.clearDisplay();
  display.drawBitmap(0, 0, failure, 128, 64, WHITE);
  display.display();
}

void display_succsess(){
  display.clearDisplay();
  display.drawBitmap(0, 0, succsess, 128, 64, WHITE);
  display.display();
}

void display_register_mode(){
  display.clearDisplay();
  display.drawBitmap(0, 0, register_mode, 128, 64, WHITE);
  display.display();
}

void display_authenticate_mode(){
  display.clearDisplay();
  display.drawBitmap(0, 0, authenticate, 128, 64, WHITE);
  display.display();
}

void display_mode_standby(){
  display.clearDisplay();
  display.drawBitmap(0, 0, standby, 128, 64, WHITE);
  display.display();
}

void display_connectionloss(){
  display.clearDisplay();
  display.drawBitmap(0, 0, conection_loss, 128, 64, WHITE);
  display.display();
}

void display_settings_mode(){
    display.clearDisplay();
    display.drawBitmap(0,0,einstellungen,128,64,WHITE);
    display.display();
}

bool containsOnlyZeroes(const String& str) {
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] != '0') {
            return false;
        }
    }
    return true;
}

void run_config_portal(){
    //reset settings - for testing
    //wm.resetSettings();
  
    // set configportal timeout
    wm.setConfigPortalTimeout(240);
    wm.setConnectTimeout(20);
    WiFiManagerParameter mqtturl("mqtturl", "Ip adress", "mqtt.olga-tech.de", 50);
    WiFiManagerParameter mqttusr("mqttusr", "MQTT User", "testuser2", 50);
    WiFiManagerParameter mqttpw("mqttpw", "MQTT PW", "testuser2", 50);
    WiFiManagerParameter mqttid("mqttid", "MQTT ID", "TestClient", 50);
    WiFiManagerParameter mqttport("mqttport","MQTT_PORT","1883",50);

    wm.addParameter(&mqtturl);
    wm.addParameter(&mqttusr);
    wm.addParameter(&mqttpw);
    wm.addParameter(&mqttid);
    wm.addParameter(&mqttport);


    if (!wm.startConfigPortal("Emma-Terminal")) {
      Serial.println("failed to connect and hit timeout");
      //reset and try again, or maybe put it to deep slee
    }

    else{
      //if you get here you have connected to the WiFi
      Serial.println("connected...yeey :)");
      preferences.putString("mqttssid", wm.getWiFiSSID(true));
      preferences.putString("mqttkey", wm.getWiFiPass(true));
    }
    
    delay(1000);

    Serial.println(mqtturl.getValue());
    Serial.println(mqttusr.getValue());
    Serial.println(mqttpw.getValue());
    Serial.println(mqttid.getValue());
    Serial.println(mqttid.getValue());

    preferences.putString("mqtturl",mqtturl.getValue());
    preferences.putString("mqttusr",mqttusr.getValue());
    preferences.putString("mqttpw",mqttpw.getValue());
    preferences.putString("mqttid",mqttid.getValue());
    preferences.putString("mqttport",mqttport.getValue());

    preferences.end();

    delay(1000);

    ESP.restart();



}

void load_flash(){
  mqtt_data.url = preferences.getString("mqtturl","");
  mqtt_data.usr= preferences.getString("mqttusr","");
  mqtt_data.pw= preferences.getString("mqttpw","");
  mqtt_data.id= preferences.getString("mqttid","");
  clientID = preferences.getString("mqttid","");
  mqtt_data.SSID = preferences.getString("mqttssid","");
  mqtt_data.KEY = preferences.getString("mqttkey","");
  mqtt_data.port = preferences.getString("mqttport","1883").toInt();

  Serial.println(mqtt_data.url);
  Serial.println(mqtt_data.usr);
  Serial.println(mqtt_data.pw);
  Serial.println(mqtt_data.id);
  Serial.println(mqtt_data.KEY);
  Serial.println(mqtt_data.SSID);
  Serial.println(mqtt_data.port);
}