#include <Arduino.h>
#include <EspMQTTClient.h>

#include <Crypto.h>
#include <AES.h>
#include <string.h>

#include <LiquidCrystal_I2C.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "Secrets.h"
#include "Utils.h"

#include "display.h"
#include "card.h"
#include "config.h"
#include "message.h"

struct
{
  String url;
  String usr;
  String pw;
  String id;
  String SSID;
  String KEY;
  int port;
} mqtt_data;

String clientID = "TestClient";

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

EspMQTTClient client;

// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;

// set LCD address, number of columns and rows
// if you don't know your display address, run an I2C scanner sketch
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

enum state
{
  WAITING_FOR_USER_ID,
  WAITING_FOR_USER_BUFFER,
} current_state;

int last_state_change;

char last_will[200] = {}; // to fix wierd pointer issure with the last will message
const char *last_msg = last_will;

enum mode
{
  NONE,
  AUTHENTICATE,
  REGISTER,
} current_mode;

AES128 aes128;

void onConnectionEstablished();
void handleCommand(const String &command);
void handleDisplay(const String &payload);
void handleData(const String &payload);
bool containsOnlyZeroes(const String &str);
void load_flash();

void setup()
{ // Open USB serial port
  Serial.begin(115200);

  // initialize LCD
  lcd.init();
  // turn on LCD backlight
  lcd.backlight();

  display_clear();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  pinMode(WM_TRIGGER_PIN, INPUT_PULLUP);
  Serial.println(wm.getWiFiSSID());
  Serial.println(wm.getWiFiPass());

  preferences.begin("my-app", false);

  if (digitalRead(WM_TRIGGER_PIN) == LOW)
  {
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    display_settings_mode();
    run_config_portal();
  }
  load_flash();
  preferences.end();

  client.setWifiCredentials(mqtt_data.SSID.c_str(), mqtt_data.KEY.c_str());
  client.setMqttServer(mqtt_data.url.c_str(), mqtt_data.usr.c_str(), mqtt_data.pw.c_str(), mqtt_data.port);
  client.setMqttClientName(mqtt_data.id.c_str());

  // Software SPI is configured to run a slow clock of 10 kHz which can be transmitted over longer cables.
  gi_PN532.InitSoftwareSPI(SPI_CLK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SPI_CS_PIN, RESET_PIN);

  pinMode(learn, INPUT_PULLUP);

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
  // client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  //  client.enableOTA(); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
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
  message3.toCharArray(last_will, message3.length() + 1);
  client.enableLastWillMessage(last_msg, "disconnect", true); // You can activate the retain flag by setting the third parameter to true
}

void loop()
{
  client.loop();

  if (!(client.isMqttConnected() && client.isWifiConnected()))
  {
    display_connectionloss();
    return;
  }

  // Handle waiting_for_user_buffer timeout
  if (current_state == WAITING_FOR_USER_BUFFER && millis() - last_state_change > 20000)
  {
    Serial.println("Timeout while waiting for user buffer");
    current_state = WAITING_FOR_USER_ID;
    display_fail();
    delay(1000);
  }

  if (current_mode == NONE)
  {
    display_mode_standby();
    return;
  }

  if (current_state == WAITING_FOR_USER_ID && current_mode == AUTHENTICATE)
  {

    display_place_card();

    unsigned char ID[8] = {0};

    clear_kCard(&last_card);
    if (ReadCard(ID, &last_card))
    {
      // No card present in the RF field
      if (last_card.u8_UidLength > 0)
      {
        clearMessage(out_message);
        memcpy(out_message.user_id, ID, USER_ID_LENGTH + 1);
        serializeMessage(out_buffer, out_message);
        client.publish("device/" + clientID + "/data", (char *)out_buffer);
        current_state = WAITING_FOR_USER_BUFFER;
        printUnsignedCharArrayAsHex(ID, 8);
        last_state_change = millis();
        display_processing();
      }

      else
      {
        gu64_LastID = 0;
      }
    }

    else
    {

      if (IsDesfireTimeout())
      {
        display_fail();
        delay(1000);
        display_place_card();
        // Nothing to do here because IsDesfireTimeout() prints additional error message and blinks the red LED
      }
      else if (last_card.b_PN532_Error) // Another error from PN532 -> reset the chip
      {
        display_fail();
        delay(1000);
        display_place_card();
        InitReader(true); // flash red LED for 2.4 seconds
      }
      else // e.g. Error while authenticating with master key
      {
        display_fail();
        delay(1000);
        display_place_card();
        // FlashLED(LED_RED, 1000);
      }

      Utils::Print("> ");
    }
  }
}

void onConnectionEstablished()
{
  // Subscribe to "device/<client>/receive/data" and display received message to Serial
  client.subscribe("device/" + clientID + "/receive/data", [](const String &payload)
                   { handleData(payload); }, qos);

  // Subscribe to "device/<client>/receive/command" and handle command
  client.subscribe("device/" + clientID + "/receive/command", [](const String &payload)
                   { handleCommand(payload); }, qos);

  // Subscribe to "device/<client>/receive/display" and display received message to Serial
  client.subscribe("device/" + clientID + "/receive/display", [](const String &payload)
                   {
        Serial.println("Remote Display Command");
        handleDisplay(payload); }, qos);

  // Publish a message to "device/<client>/register"
  client.publish("device/" + clientID + "/register", "connect", true); // You can activate the retain flag by setting the third parameter to true
}

void handleCommand(const String &command)
{
  Serial.println("Received command: " + command);
  if (command == "authenticate")
  {
    Serial.println("Enable Authenticate Mode");
    display_authenticate_mode();
    current_mode = AUTHENTICATE;
  }
  else if (command == "register")
  {
    Serial.println("Enable Register Mode");
    display_register_mode();
    current_mode = REGISTER;
  }
  else if (command == "idle")
  {
    Serial.println("Disable Mode");
    display_mode_standby();
    current_mode = NONE;
  }
  else if (command == "reset")
  {
    Serial.println("Resetting");
    ESP.restart();
  }
  else
  {
    Serial.println("Unknown command");
  }
}

void handleDisplay(const String &payload)
{
  // Make a copy of the payload string
  char buffer[payload.length() + 1];

  payload.toCharArray(buffer, sizeof(buffer));
  Serial.println(buffer);
  char line1[17] = {};
  char line2[17] = {};

  deserializeMessage_for_disp(buffer, line1, line2);

  Serial.println(line1);
  Serial.println(line2);

  if (containsOnlyZeroes(line1) && containsOnlyZeroes(line2))
  {
    display_clear();
  }
  else
  {
    display_lines(line1, line2);
  }
}

void handleData(const String &payload)
{
  // Make a copy of the payload string
  char buffer[payload.length() + 1];
  payload.toCharArray(buffer, sizeof(buffer));

  deserializeMessage(buffer, in_message);

  switch (current_mode)
  {
  case NONE:
    Serial.println("No Mode");
    break;
  case AUTHENTICATE:

    Serial.println("Authenticate Mode");
    // do authentication stuff

    if (current_state == WAITING_FOR_USER_BUFFER)
    {
      unsigned char key[enc_key_length] = {0};
      clearMessage(out_message);

      if (authenticate_user(in_message.user_id, reinterpret_cast<char *>(in_message.user_buffer), &last_card, key))
      {
        aes128.setKey(key, enc_key_length);
        unsigned char encr_data[16] = {0};
        aes128.encryptBlock(encr_data, in_message.encryption_data);
        memcpy(out_message.user_id, in_message.user_id, USER_ID_LENGTH + 1);
        memcpy(out_message.encryption_data, encr_data, ENCRYPTION_DATA_LENGTH);
        serializeMessage(out_buffer, out_message);
        client.publish("device/" + clientID + "/data", (char *)out_buffer);
        display_success();
        delay(1000);
      }

      else
      {
        memcpy(out_message.user_id, in_message.user_id, USER_ID_LENGTH + 1);
        memcpy(out_message.encryption_data, "Failed_to_authe", ENCRYPTION_DATA_LENGTH);
        serializeMessage(out_buffer, out_message);
        client.publish("device/" + clientID + "/data", (char *)out_buffer);
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
    if (current_mode == REGISTER)
    {
      if (customize_card(reinterpret_cast<char *>(in_message.user_buffer), in_message.data, out_message.user_id))
      {
        printUnsignedCharArrayAsHex(in_message.data, 16);
        printUnsignedCharArrayAsHex(in_message.encryption_data, 16);
        aes128.setKey(in_message.data, enc_key_length);
        unsigned char encr_data[16] = {0};
        aes128.encryptBlock(encr_data, in_message.encryption_data);
        printUnsignedCharArrayAsHex(encr_data, 16);
        memcpy(out_message.encryption_data, encr_data, ENCRYPTION_DATA_LENGTH);
        serializeMessage(out_buffer, out_message);
        client.publish("device/" + clientID + "/data", (char *)out_buffer);
        display_success();
        delay(1000);
        current_state = WAITING_FOR_USER_ID;
        current_mode = NONE;
      }
    }
    else
    {
      // Failed to assign card
      memcpy(out_message.user_id, in_message.user_id, USER_ID_LENGTH + 1);
      memcpy(out_message.encryption_data, "Failed_to_assig", ENCRYPTION_DATA_LENGTH);
      serializeMessage(out_buffer, out_message);
      client.publish("device/" + clientID + "/data", (char *)out_buffer);
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

bool containsOnlyZeroes(const String &str)
{
  for (size_t i = 0; i < str.length(); ++i)
  {
    if (str[i] != '0')
    {
      return false;
    }
  }
  return true;
}

void load_flash()
{
  mqtt_data.url = preferences.getString("mqtturl", "");
  mqtt_data.usr = preferences.getString("mqttusr", "");
  mqtt_data.pw = preferences.getString("mqttpw", "");
  mqtt_data.id = preferences.getString("mqttid", "");
  clientID = preferences.getString("mqttid", "");
  mqtt_data.SSID = preferences.getString("mqttssid", "");
  mqtt_data.KEY = preferences.getString("mqttkey", "");
  mqtt_data.port = preferences.getString("mqttport", "1883").toInt();

  Serial.println(mqtt_data.url);
  Serial.println(mqtt_data.usr);
  Serial.println(mqtt_data.pw);
  Serial.println(mqtt_data.id);
  Serial.println(mqtt_data.KEY);
  Serial.println(mqtt_data.SSID);
  Serial.println(mqtt_data.port);
}