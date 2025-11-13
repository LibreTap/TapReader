#include "mqtt_types.h"

// Command Type conversions
const char* commandTypeToString(CommandType type) {
    switch (type) {
        case CommandType::REGISTER_START: return "register_start";
        case CommandType::REGISTER_CANCEL: return "register_cancel";
        case CommandType::AUTH_START: return "auth_start";
        case CommandType::AUTH_VERIFY: return "auth_verify";
        case CommandType::AUTH_CANCEL: return "auth_cancel";
        case CommandType::READ_START: return "read_start";
        case CommandType::READ_CANCEL: return "read_cancel";
        case CommandType::RESET: return "reset";
        default: return "unknown";
    }
}

CommandType stringToCommandType(const char* str) {
    if (strcmp(str, "register_start") == 0) return CommandType::REGISTER_START;
    if (strcmp(str, "register_cancel") == 0) return CommandType::REGISTER_CANCEL;
    if (strcmp(str, "auth_start") == 0) return CommandType::AUTH_START;
    if (strcmp(str, "auth_verify") == 0) return CommandType::AUTH_VERIFY;
    if (strcmp(str, "auth_cancel") == 0) return CommandType::AUTH_CANCEL;
    if (strcmp(str, "read_start") == 0) return CommandType::READ_START;
    if (strcmp(str, "read_cancel") == 0) return CommandType::READ_CANCEL;
    if (strcmp(str, "reset") == 0) return CommandType::RESET;
    return CommandType::UNKNOWN;
}

// Event Type conversions
const char* eventTypeToString(EventType type) {
    switch (type) {
        case EventType::REGISTER_SUCCESS: return "register_success";
        case EventType::REGISTER_ERROR: return "register_error";
        case EventType::AUTH_TAG_DETECTED: return "auth_tag_detected";
        case EventType::AUTH_SUCCESS: return "auth_success";
        case EventType::AUTH_FAILED: return "auth_failed";
        case EventType::AUTH_ERROR: return "auth_error";
        case EventType::READ_SUCCESS: return "read_success";
        case EventType::READ_ERROR: return "read_error";
        case EventType::STATUS_CHANGE: return "status_change";
        case EventType::MODE_CHANGE: return "mode_change";
        case EventType::HEARTBEAT: return "heartbeat";
        default: return "unknown";
    }
}

EventType stringToEventType(const char* str) {
    if (strcmp(str, "register_success") == 0) return EventType::REGISTER_SUCCESS;
    if (strcmp(str, "register_error") == 0) return EventType::REGISTER_ERROR;
    if (strcmp(str, "auth_tag_detected") == 0) return EventType::AUTH_TAG_DETECTED;
    if (strcmp(str, "auth_success") == 0) return EventType::AUTH_SUCCESS;
    if (strcmp(str, "auth_failed") == 0) return EventType::AUTH_FAILED;
    if (strcmp(str, "auth_error") == 0) return EventType::AUTH_ERROR;
    if (strcmp(str, "read_success") == 0) return EventType::READ_SUCCESS;
    if (strcmp(str, "read_error") == 0) return EventType::READ_ERROR;
    if (strcmp(str, "status_change") == 0) return EventType::STATUS_CHANGE;
    if (strcmp(str, "mode_change") == 0) return EventType::MODE_CHANGE;
    if (strcmp(str, "heartbeat") == 0) return EventType::HEARTBEAT;
    return EventType::UNKNOWN;
}

// Device Mode conversions
const char* deviceModeToString(DeviceMode mode) {
    switch (mode) {
        case DeviceMode::IDLE: return "idle";
        case DeviceMode::REGISTER: return "register";
        case DeviceMode::AUTH: return "auth";
        case DeviceMode::READ: return "read";
        default: return "idle";
    }
}

DeviceMode stringToDeviceMode(const char* str) {
    if (strcmp(str, "idle") == 0) return DeviceMode::IDLE;
    if (strcmp(str, "register") == 0) return DeviceMode::REGISTER;
    if (strcmp(str, "auth") == 0) return DeviceMode::AUTH;
    if (strcmp(str, "read") == 0) return DeviceMode::READ;
    return DeviceMode::IDLE;
}

// Device Status conversions
const char* deviceStatusToString(DeviceStatus status) {
    switch (status) {
        case DeviceStatus::ONLINE: return "online";
        case DeviceStatus::OFFLINE: return "offline";
        default: return "offline";
    }
}

DeviceStatus stringToDeviceStatus(const char* str) {
    if (strcmp(str, "online") == 0) return DeviceStatus::ONLINE;
    if (strcmp(str, "offline") == 0) return DeviceStatus::OFFLINE;
    return DeviceStatus::OFFLINE;
}

// Error Code conversions
const char* errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::NFC_TIMEOUT: return "NFC_TIMEOUT";
        case ErrorCode::NFC_TAG_LOST: return "NFC_TAG_LOST";
        case ErrorCode::NFC_AUTH_FAILED: return "NFC_AUTH_FAILED";
        case ErrorCode::NFC_READ_ERROR: return "NFC_READ_ERROR";
        case ErrorCode::NFC_WRITE_ERROR: return "NFC_WRITE_ERROR";
        case ErrorCode::NFC_UNSUPPORTED_TAG: return "NFC_UNSUPPORTED_TAG";
        case ErrorCode::NFC_INVALID_KEY: return "NFC_INVALID_KEY";
        case ErrorCode::NFC_DEVICE_BUSY: return "NFC_DEVICE_BUSY";
        case ErrorCode::NFC_DEVICE_ERROR: return "NFC_DEVICE_ERROR";
        case ErrorCode::INVALID_COMMAND: return "INVALID_COMMAND";
        case ErrorCode::SESSION_NOT_FOUND: return "SESSION_NOT_FOUND";
        case ErrorCode::TIMEOUT_EXCEEDED: return "TIMEOUT_EXCEEDED";
        default: return "UNKNOWN";
    }
}

ErrorCode stringToErrorCode(const char* str) {
    if (strcmp(str, "NFC_TIMEOUT") == 0) return ErrorCode::NFC_TIMEOUT;
    if (strcmp(str, "NFC_TAG_LOST") == 0) return ErrorCode::NFC_TAG_LOST;
    if (strcmp(str, "NFC_AUTH_FAILED") == 0) return ErrorCode::NFC_AUTH_FAILED;
    if (strcmp(str, "NFC_READ_ERROR") == 0) return ErrorCode::NFC_READ_ERROR;
    if (strcmp(str, "NFC_WRITE_ERROR") == 0) return ErrorCode::NFC_WRITE_ERROR;
    if (strcmp(str, "NFC_UNSUPPORTED_TAG") == 0) return ErrorCode::NFC_UNSUPPORTED_TAG;
    if (strcmp(str, "NFC_INVALID_KEY") == 0) return ErrorCode::NFC_INVALID_KEY;
    if (strcmp(str, "NFC_DEVICE_BUSY") == 0) return ErrorCode::NFC_DEVICE_BUSY;
    if (strcmp(str, "NFC_DEVICE_ERROR") == 0) return ErrorCode::NFC_DEVICE_ERROR;
    if (strcmp(str, "INVALID_COMMAND") == 0) return ErrorCode::INVALID_COMMAND;
    if (strcmp(str, "SESSION_NOT_FOUND") == 0) return ErrorCode::SESSION_NOT_FOUND;
    if (strcmp(str, "TIMEOUT_EXCEEDED") == 0) return ErrorCode::TIMEOUT_EXCEEDED;
    return ErrorCode::UNKNOWN;
}

// Error Component conversions
const char* errorComponentToString(ErrorComponent component) {
    switch (component) {
        case ErrorComponent::NFC: return "nfc";
        case ErrorComponent::PN532: return "pn532";
        case ErrorComponent::CRYPTO: return "crypto";
        case ErrorComponent::DEVICE: return "device";
        case ErrorComponent::PROTOCOL: return "protocol";
        default: return "device";
    }
}

ErrorComponent stringToErrorComponent(const char* str) {
    if (strcmp(str, "nfc") == 0) return ErrorComponent::NFC;
    if (strcmp(str, "pn532") == 0) return ErrorComponent::PN532;
    if (strcmp(str, "crypto") == 0) return ErrorComponent::CRYPTO;
    if (strcmp(str, "device") == 0) return ErrorComponent::DEVICE;
    if (strcmp(str, "protocol") == 0) return ErrorComponent::PROTOCOL;
    return ErrorComponent::DEVICE;
}
