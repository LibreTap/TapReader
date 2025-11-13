#pragma once

#ifndef UNIT_TEST
#include <Arduino.h>
#endif
#include <ArduinoJson.h>
#include "mqtt_types.h"

// Message Envelope - base structure for all MQTT messages
struct MQTTMessageEnvelope {
    char version[8];                            // Protocol version "1.0"
    char timestamp[32];                         // ISO 8601 UTC timestamp
    char device_id[MAX_DEVICE_ID_LENGTH + 1];  // Device identifier
    char event_type[32];                        // Message type string
    char request_id[MAX_UUID_LENGTH + 1];      // UUID v4 for operation tracking
    JsonObject payload;                         // Event-specific payload data
};

// User Data for authentication
struct UserData {
    char username[MAX_USERNAME_LENGTH + 1];
    char context[MAX_CONTEXT_LENGTH + 1];
    
    void clear() {
        memset(username, 0, sizeof(username));
        memset(context, 0, sizeof(context));
    }
};

// Command Payloads (Service → Device)

// Register Start Command
struct RegisterStartPayload {
    char tag_uid[MAX_TAG_UID_LENGTH + 1];      // NFC tag UID (colon-separated hex)
    char key[MAX_HEX_KEY_LENGTH + 1];          // 32-character hex encryption key
    int timeout_seconds;                        // Operation timeout in seconds
    
    void clear() {
        memset(tag_uid, 0, sizeof(tag_uid));
        memset(key, 0, sizeof(key));
        timeout_seconds = 0;
    }
};

// Auth Start Command
struct AuthStartPayload {
    int timeout_seconds;                        // Operation timeout in seconds
    
    void clear() {
        timeout_seconds = 0;
    }
};

// Auth Verify Command
struct AuthVerifyPayload {
    char tag_uid[MAX_TAG_UID_LENGTH + 1];      // NFC tag UID
    char key[MAX_HEX_KEY_LENGTH + 1];          // 32-character hex encryption key
    UserData user_data;                         // User authentication data
    
    void clear() {
        memset(tag_uid, 0, sizeof(tag_uid));
        memset(key, 0, sizeof(key));
        user_data.clear();
    }
};

// Read Start Command
struct ReadStartPayload {
    int timeout_seconds;                        // Operation timeout in seconds
    uint8_t read_blocks[256];                   // Block numbers to read from tag
    size_t read_blocks_count;                   // Number of blocks to read
    
    void clear() {
        timeout_seconds = 0;
        memset(read_blocks, 0, sizeof(read_blocks));
        read_blocks_count = 0;
    }
};

// Event Payloads (Device → Service)

// Status Change Event
struct StatusChangePayload {
    DeviceStatus status;                        // online or offline
    char firmware_version[MAX_FIRMWARE_VERSION_LENGTH + 1];  // Semantic version
    char ip_address[MAX_IP_ADDRESS_LENGTH + 1]; // IPv4 address
    
    void clear() {
        status = DeviceStatus::OFFLINE;
        memset(firmware_version, 0, sizeof(firmware_version));
        memset(ip_address, 0, sizeof(ip_address));
    }
};

// Mode Change Event
struct ModeChangePayload {
    DeviceMode mode;                            // Current mode
    DeviceMode previous_mode;                   // Previous mode
    
    void clear() {
        mode = DeviceMode::IDLE;
        previous_mode = DeviceMode::IDLE;
    }
};

// Tag Detected Event
struct TagDetectedPayload {
    char tag_uid[MAX_TAG_UID_LENGTH + 1];      // NFC tag UID
    char message[MAX_ERROR_MESSAGE_LENGTH + 1]; // Human-readable status
    
    void clear() {
        memset(tag_uid, 0, sizeof(tag_uid));
        memset(message, 0, sizeof(message));
    }
};

// Register Success Event
struct RegisterSuccessPayload {
    char tag_uid[MAX_TAG_UID_LENGTH + 1];      // Tag UID
    int blocks_written;                         // Number of blocks written
    char message[MAX_ERROR_MESSAGE_LENGTH + 1]; // Success message
    
    void clear() {
        memset(tag_uid, 0, sizeof(tag_uid));
        blocks_written = 0;
        memset(message, 0, sizeof(message));
    }
};

// Auth Success Event
struct AuthSuccessPayload {
    char tag_uid[MAX_TAG_UID_LENGTH + 1];      // Tag UID
    bool authenticated;                         // Must be true
    char message[MAX_ERROR_MESSAGE_LENGTH + 1]; // Success message
    UserData user_data;                         // User authentication data
    
    void clear() {
        memset(tag_uid, 0, sizeof(tag_uid));
        authenticated = false;
        memset(message, 0, sizeof(message));
        user_data.clear();
    }
};

// Auth Failed Event
struct AuthFailedPayload {
    char tag_uid[MAX_TAG_UID_LENGTH + 1];      // Tag UID
    bool authenticated;                         // Must be false
    char reason[MAX_ERROR_MESSAGE_LENGTH + 1];  // Failure reason
    
    void clear() {
        memset(tag_uid, 0, sizeof(tag_uid));
        authenticated = false;
        memset(reason, 0, sizeof(reason));
    }
};

// Error Event
struct ErrorPayload {
    char error[MAX_ERROR_MESSAGE_LENGTH + 1];   // Human-readable error description
    ErrorCode error_code;                        // Machine-parseable error code
    bool retry_possible;                         // Whether operation can be retried
    ErrorComponent component;                    // Component that generated error
    
    void clear() {
        memset(error, 0, sizeof(error));
        error_code = ErrorCode::UNKNOWN;
        retry_possible = false;
        component = ErrorComponent::DEVICE;
    }
};

// Heartbeat Event
struct HeartbeatPayload {
    unsigned long uptime_seconds;               // Device uptime in seconds
    float memory_usage_percent;                 // Memory usage percentage
    unsigned int operations_completed;          // Total operations completed
    
    void clear() {
        uptime_seconds = 0;
        memory_usage_percent = 0.0;
        operations_completed = 0;
    }
};

// Read Success Event
struct ReadSuccessPayload {
    char tag_uid[MAX_TAG_UID_LENGTH + 1];      // Tag UID
    char message[MAX_ERROR_MESSAGE_LENGTH + 1]; // Success message
    uint8_t blocks_read[256];                   // Blocks that were read
    size_t blocks_read_count;                   // Number of blocks read
    
    void clear() {
        memset(tag_uid, 0, sizeof(tag_uid));
        memset(message, 0, sizeof(message));
        memset(blocks_read, 0, sizeof(blocks_read));
        blocks_read_count = 0;
    }
};
