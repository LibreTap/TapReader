#include <Arduino.h>
#include <ArduinoJson.h>
#include <base64.hpp>
#include <EspMQTTClient.h>

#define USER_ID_LENGTH 7
#define USER_BUFFER_LENGTH 32
#define DATA_LENGTH 32
#define ENCRYPTION_DATA_LENGTH 32

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

const String clientID = "TestClient";

enum state {
  WAITING_FOR_USER_ID,
  WAITING_FOR_USER_BUFFER,
} current_state;
int last_state_change;

enum mode {
  NONE,
  AUTHENTICATE,
  REGISTER,
} current_mode;

EspMQTTClient client(
  "ssid",      // Wifi ssid
  "password",  // Wifi password
  "127.0.0.1",  // MQTT Broker server ip
  // "MQTTUsername",   // Can be omitted if not needed
  // "MQTTPassword",   // Can be omitted if not needed
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
  unsigned char data[DATA_LENGTH + 1];
  unsigned char encryption_data[ENCRYPTION_DATA_LENGTH + 1];
};

Message in_message;                         // <- global configuration object
Message out_message;
unsigned char out_buffer[BUFFER_LENGTH + 1];

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
  doc[DATA_KEY] = data;
  doc[ENCRYPTION_DATA_KEY] = encryption_data;

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

  decode(doc[USER_ID_KEY], USER_ID_LENGTH, message.user_id);
  decode(doc[USER_BUFFER_KEY], USER_BUFFER_LENGTH, message.user_buffer);
  decode(doc[DATA_KEY], DATA_LENGTH, message.data);
  decode(doc[ENCRYPTION_DATA_KEY], ENCRYPTION_DATA_LENGTH, message.encryption_data);
}


void handleCommand(const String & command) {
  Serial.println("Received command: " + command);
  if (command == "authenticate") {
    Serial.println("Enable Authenticate Mode");
    current_mode = AUTHENTICATE;
  } else if (command == "register") {
    Serial.println("Enable Register Mode");
    current_mode = REGISTER;
  } else if (command == "none") {
    Serial.println("Disable Mode");
    current_mode = NONE;
  } else if (command == "reset") {
    Serial.println("Resetting");
    ESP.restart();
  } else {
    Serial.println("Unknown command");
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
      clearMessage(out_message);
      memcpy(out_message.user_id, in_message.user_id, USER_ID_LENGTH);
      memcpy(out_message.encryption_data, "enrypted_data_for_verification", ENCRYPTION_DATA_LENGTH);
      serializeMessage(out_buffer, out_message);
      client.publish("device/" + clientID + "/data", (char*)out_buffer);
      current_state = WAITING_FOR_USER_ID;
      current_mode = NONE;
      break;
    case REGISTER:
      Serial.println("Register Mode");
      // do register stuff
      clearMessage(out_message);
      memcpy(out_message.user_id, in_message.user_id, USER_ID_LENGTH);
      memcpy(out_message.encryption_data, "enrypted_data_for_verification", ENCRYPTION_DATA_LENGTH);
      serializeMessage(out_buffer, out_message);
      client.publish("device/" + clientID + "/data", (char*)out_buffer);
      current_state = WAITING_FOR_USER_ID;
      current_mode = NONE;
      break;
    default:
      Serial.println("Unknown mode");
  }
}

void setup()
{
  Serial.begin(115200);

  current_state = WAITING_FOR_USER_ID;
  last_state_change = millis();
  current_mode = NONE;

  // Optional functionalities of EspMQTTClient
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  // client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  // client.enableOTA(); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
  // client.enableLastWillMessage("device/" + clientID + "/register", "disconnect");  // You can activate the retain flag by setting the third parameter to true
}

// This function is called once everything is connected (Wifi and MQTT)
// WARNING : YOU MUST IMPLEMENT IT IF YOU USE EspMQTTClient
void onConnectionEstablished()
{
  // Subscribe to "device/<client>/receive/data" and display received message to Serial
  client.subscribe("device/" + clientID + "/receive/data", [](const String & payload) {
    handleData(payload);
  });

  // Subscribe to "device/<client>/receive/command" and handle command
  client.subscribe("device/" + clientID + "/receive/command", [](const String & payload) {
    handleCommand(payload);
  });

  // Subscribe to "device/<client>/receive/display" and display received message to Serial
  client.subscribe("device/" + clientID + "/receive/display", [](const String & payload) {
    Serial.println("Display data: " + payload);
  });

  // Publish a message to "device/<client>/register"
  client.publish("device/" + clientID + "/register", "connect"); // You can activate the retain flag by setting the third parameter to true
}

void loop()
{
  client.loop();

  // Handle waiting_for_user_buffer timeout
  if (current_state == WAITING_FOR_USER_BUFFER && millis() - last_state_change > 20000) {
    Serial.println("Timeout while waiting for user buffer");
    current_state = WAITING_FOR_USER_ID;
  }

  if (current_mode == NONE) {
    return;
  }

  // Mockup for reading user ids from nfc card
  String input = Serial.readStringUntil('\n');
  if (current_state == WAITING_FOR_USER_ID && input.length() > 0) {
    clearMessage(out_message);
    memcpy(out_message.user_id, reinterpret_cast<const unsigned char*>(input.c_str()), USER_ID_LENGTH);    serializeMessage(out_buffer, out_message);
    client.publish("device/" + clientID + "/data", (char*)out_buffer);
    current_state = WAITING_FOR_USER_BUFFER;
    last_state_change = millis();
  }
}
