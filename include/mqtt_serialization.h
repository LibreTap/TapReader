#pragma once

#ifndef UNIT_TEST
#include <Arduino.h>
#endif
#include <ArduinoJson.h>
#include "mqtt_schema.h"
#include "mqtt_types.h"

// Document sizes for JSON serialization
// Calculated using https://arduinojson.org/v6/assistant/
#define MQTT_ENVELOPE_DOC_SIZE 512
#define MQTT_COMMAND_DOC_SIZE 768
#define MQTT_EVENT_DOC_SIZE 1024

// Utility functions for generating timestamps and UUIDs
void generateTimestamp(char* buffer, size_t bufferSize);
void generateUUID(char* buffer, size_t bufferSize);

// Message Envelope serialization
bool serializeEnvelope(const MQTTMessageEnvelope& envelope, char* jsonBuffer, size_t bufferSize);
bool deserializeEnvelope(const char* jsonBuffer, MQTTMessageEnvelope& envelope, StaticJsonDocument<MQTT_ENVELOPE_DOC_SIZE>& doc);

// Command Payload Deserialization (Service → Device)
bool deserializeRegisterStart(JsonObject payload, RegisterStartPayload& data);
bool deserializeAuthStart(JsonObject payload, AuthStartPayload& data);
bool deserializeAuthVerify(JsonObject payload, AuthVerifyPayload& data);
bool deserializeReadStart(JsonObject payload, ReadStartPayload& data);

// Event Payload Serialization (Device → Service)
bool serializeStatusChange(JsonObject payload, const StatusChangePayload& data);
bool serializeModeChange(JsonObject payload, const ModeChangePayload& data);
bool serializeTagDetected(JsonObject payload, const TagDetectedPayload& data);
bool serializeRegisterSuccess(JsonObject payload, const RegisterSuccessPayload& data);
bool serializeAuthSuccess(JsonObject payload, const AuthSuccessPayload& data);
bool serializeAuthFailed(JsonObject payload, const AuthFailedPayload& data);
bool serializeError(JsonObject payload, const ErrorPayload& data);
bool serializeHeartbeat(JsonObject payload, const HeartbeatPayload& data);
bool serializeReadSuccess(JsonObject payload, const ReadSuccessPayload& data);

// Helper functions for payload serialization/deserialization
bool serializeUserData(JsonObject obj, const UserData& userData);
bool deserializeUserData(JsonObject obj, UserData& userData);

// Validation functions
bool isValidTagUID(const char* uid);
bool isValidHexKey(const char* key);
bool isValidUUID(const char* uuid);
bool isValidDeviceId(const char* deviceId);
