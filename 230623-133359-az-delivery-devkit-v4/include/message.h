#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"

// Our configuration structure.
//
// Never use a JsonDocument to store the configuration!
// A JsonDocument is *not* a permanent storage; it's only a temporary storage
// used during the serialization phase. See:
// https://arduinojson.org/v6/faq/why-must-i-create-a-separate-config-object/
struct Message
{
  unsigned char user_id[USER_ID_LENGTH + 1];
  unsigned char user_buffer[USER_BUFFER_LENGTH + 1];
  unsigned char data[DATA_LENGTH + 1];                       // for transmitting the key
  unsigned char encryption_data[ENCRYPTION_DATA_LENGTH + 1]; // for getting random data and sending back the encipted
};

// Buffer/message utility functions
void clearBuffer(unsigned char *buffer, size_t size);
void clearMessage(Message &message);
void encode(const unsigned char *buffer, size_t size, unsigned char *out_buffer);
void decode(const char *buffer, size_t buffer_size, unsigned char *out_buffer);
void serializeMessage(unsigned char *buffer, const Message &message);
void deserializeMessage(char *buffer, Message &message);
void deserializeMessage_for_disp(char *buffer, char *line1, char *line2);

extern Message in_message;
extern Message out_message;
extern unsigned char out_buffer[BUFFER_LENGTH + 1];
