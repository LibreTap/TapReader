#pragma once

#ifndef UNIT_TEST
#include <Arduino.h>
#endif
#include <ArduinoJson.h>

// MQTT Protocol Version
#define MQTT_PROTOCOL_VERSION "1.0"

// Maximum lengths for various fields
#define MAX_TAG_UID_LENGTH 32
#define MAX_HEX_KEY_LENGTH 32
#define MAX_DEVICE_ID_LENGTH 64
#define MAX_UUID_LENGTH 36
#define MAX_ERROR_MESSAGE_LENGTH 256
#define MAX_USERNAME_LENGTH 128
#define MAX_CONTEXT_LENGTH 256
#define MAX_FIRMWARE_VERSION_LENGTH 16
#define MAX_IP_ADDRESS_LENGTH 16

// Event Types - Commands (Service → Device)
enum class CommandType {
    REGISTER_START,
    REGISTER_CANCEL,
    AUTH_START,
    AUTH_VERIFY,
    AUTH_CANCEL,
    READ_START,
    READ_CANCEL,
    RESET,
    UNKNOWN
};

// Event Types - Events (Device → Service)
enum class EventType {
    REGISTER_SUCCESS,
    REGISTER_ERROR,
    AUTH_TAG_DETECTED,
    AUTH_SUCCESS,
    AUTH_FAILED,
    AUTH_ERROR,
    READ_SUCCESS,
    READ_ERROR,
    STATUS_CHANGE,
    MODE_CHANGE,
    HEARTBEAT,
    UNKNOWN
};

// Device Modes
enum class DeviceMode {
    IDLE,
    REGISTER,
    AUTH,
    READ
};

// Device Status
enum class DeviceStatus {
    ONLINE,
    OFFLINE
};

// Error Codes
enum class ErrorCode {
    NFC_TIMEOUT,
    NFC_TAG_LOST,
    NFC_AUTH_FAILED,
    NFC_READ_ERROR,
    NFC_WRITE_ERROR,
    NFC_UNSUPPORTED_TAG,
    NFC_INVALID_KEY,
    NFC_DEVICE_BUSY,
    NFC_DEVICE_ERROR,
    INVALID_COMMAND,
    SESSION_NOT_FOUND,
    TIMEOUT_EXCEEDED,
    UNKNOWN
};

// Error Component
enum class ErrorComponent {
    NFC,
    PN532,
    CRYPTO,
    DEVICE,
    PROTOCOL
};

// Helper functions to convert enums to/from strings
const char* commandTypeToString(CommandType type);
CommandType stringToCommandType(const char* str);

const char* eventTypeToString(EventType type);
EventType stringToEventType(const char* str);

const char* deviceModeToString(DeviceMode mode);
DeviceMode stringToDeviceMode(const char* str);

const char* deviceStatusToString(DeviceStatus status);
DeviceStatus stringToDeviceStatus(const char* str);

const char* errorCodeToString(ErrorCode code);
ErrorCode stringToErrorCode(const char* str);

const char* errorComponentToString(ErrorComponent component);
ErrorComponent stringToErrorComponent(const char* str);
