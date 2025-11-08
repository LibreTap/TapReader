#include "message.h"
#include <base64.hpp>

Message in_message;
Message out_message;
unsigned char out_buffer[BUFFER_LENGTH + 1];

void clearBuffer(unsigned char *buffer, size_t size)
{
    for (int i = 0; i < size; i++)
    {
        buffer[i] = '\0';
    }
}

void clearMessage(Message &message)
{
    clearBuffer(message.user_id, USER_ID_LENGTH);
    clearBuffer(message.user_buffer, USER_BUFFER_LENGTH);
    clearBuffer(message.data, DATA_LENGTH);
    clearBuffer(message.encryption_data, ENCRYPTION_DATA_LENGTH);
}

void encode(const unsigned char *buffer, size_t size, unsigned char *out_buffer)
{
    // encode_base64() places a null terminator automatically, because the output is a string
    int length = 0;
    while (length < size && buffer[length] != '\0')
    {
        length++;
    }
    encode_base64(buffer, length, out_buffer);
}

void decode(const char *buffer, size_t buffer_size, unsigned char *out_buffer)
{
    unsigned char copy_buffer[buffer_size + 1];
    strlcpy((char *)copy_buffer, buffer, buffer_size + 1);
    // decode_base64() does not place a null terminator, because the output is not always a string
    unsigned int string_length = decode_base64(copy_buffer, buffer_size, out_buffer);
    out_buffer[string_length] = '\0';
}

void serializeMessage(unsigned char *buffer, const Message &message)
{
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
    doc[DATA_KEY] = data;                       // for transmitting the key
    doc[ENCRYPTION_DATA_KEY] = encryption_data; // for getting random data and sending back the encrypted

    // Serialize JSON to serial
    if (serializeJson(doc, Serial) == 0)
    {
        Serial.println(F("Failed to serialize message"));
    }

    // Serialize JSON to buffer
    if (serializeJson(doc, buffer, BUFFER_LENGTH) == 0)
    {
        Serial.println(F("Failed to serialize message"));
    }
}

void deserializeMessage(char *buffer, Message &message)
{
    clearMessage(message);

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use https://arduinojson.org/assistant to compute the capacity.
    StaticJsonDocument<DOCUMENT_LENGTH> doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, buffer, BUFFER_LENGTH);
    if (error)
    {
        Serial.println(F("Failed to read message"));
        return;
    }

    decode(doc[USER_ID_KEY], BASE64_USER_ID_LENGTH, message.user_id);
    decode(doc[USER_BUFFER_KEY], BASE64_USER_BUFFER_LENGTH, message.user_buffer);
    decode(doc[DATA_KEY], BASE64_DATA_LENGTH, message.data);
    decode(doc[ENCRYPTION_DATA_KEY], BASE64_ENCRYPTION_DATA_LENGTH, message.encryption_data);
}

void deserializeMessage_for_disp(char *buffer, char *line1, char *line2)
{
    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use https://arduinojson.org/assistant to compute the capacity.
    StaticJsonDocument<DOCUMENT_LENGTH> doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, buffer, BUFFER_LENGTH);
    if (error)
    {
        Serial.println(F("Failed to read message"));
        return;
    }

    // Get the values from the JSON document and copy them to the provided arrays
    strncpy(line1, doc["line1"], 16);
    strncpy(line2, doc["line2"], 16);

    line1[16] = '\0';
    line2[16] = '\0';
}