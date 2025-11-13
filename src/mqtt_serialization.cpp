#include "mqtt_serialization.h"
#include <time.h>
#include <sys/time.h>

#if defined(UNIT_TEST) && defined(ARDUINO_ARCH_NATIVE)
#include "arduino_mocks.h"
#endif

// Generate ISO 8601 UTC timestamp
void generateTimestamp(char* buffer, size_t bufferSize) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    time_t now = tv.tv_sec;
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    
    // Format: 2025-11-11T12:00:00.000Z
    snprintf(buffer, bufferSize, "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec,
             tv.tv_usec / 1000);
}

// Generate UUID v4 (simplified version for ESP32)
void generateUUID(char* buffer, size_t bufferSize) {
    // Simple UUID generation using random numbers
    // Format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    snprintf(buffer, bufferSize,
             "%08lx-%04x-4%03x-%04x-%012lx",
             esp_random(),
             (uint16_t)(esp_random() & 0xFFFF),
             (uint16_t)(esp_random() & 0xFFF),
             (uint16_t)((esp_random() & 0x3FFF) | 0x8000),
             ((uint64_t)esp_random() << 16) | (esp_random() & 0xFFFF));
}

// Serialize Message Envelope
bool serializeEnvelope(const MQTTMessageEnvelope& envelope, char* jsonBuffer, size_t bufferSize) {
    StaticJsonDocument<MQTT_ENVELOPE_DOC_SIZE> doc;
    
    doc["version"] = envelope.version;
    doc["timestamp"] = envelope.timestamp;
    doc["device_id"] = envelope.device_id;
    doc["event_type"] = envelope.event_type;
    doc["request_id"] = envelope.request_id;
    
    // Payload will be added separately by caller
    JsonObject payload = doc.createNestedObject("payload");
    
    size_t size = serializeJson(doc, jsonBuffer, bufferSize);
    return size > 0;
}

// Deserialize Message Envelope
bool deserializeEnvelope(const char* jsonBuffer, MQTTMessageEnvelope& envelope, StaticJsonDocument<MQTT_ENVELOPE_DOC_SIZE>& doc) {
    DeserializationError error = deserializeJson(doc, jsonBuffer);
    if (error) {
        Serial.print(F("deserializeEnvelope failed: "));
        Serial.println(error.c_str());
        return false;
    }
    
    // Validate required fields
    if (!doc.containsKey("version") || !doc.containsKey("timestamp") || 
        !doc.containsKey("device_id") || !doc.containsKey("event_type") ||
        !doc.containsKey("request_id") || !doc.containsKey("payload")) {
        Serial.println(F("Missing required envelope fields"));
        return false;
    }
    
    strlcpy(envelope.version, doc["version"] | "", sizeof(envelope.version));
    strlcpy(envelope.timestamp, doc["timestamp"] | "", sizeof(envelope.timestamp));
    strlcpy(envelope.device_id, doc["device_id"] | "", sizeof(envelope.device_id));
    strlcpy(envelope.event_type, doc["event_type"] | "", sizeof(envelope.event_type));
    strlcpy(envelope.request_id, doc["request_id"] | "", sizeof(envelope.request_id));
    
    envelope.payload = doc["payload"].as<JsonObject>();
    
    return true;
}

// Deserialize Register Start Command
bool deserializeRegisterStart(JsonObject payload, RegisterStartPayload& data) {
    data.clear();
    
    if (!payload.containsKey("tag_uid") || !payload.containsKey("key") || 
        !payload.containsKey("timeout_seconds")) {
        return false;
    }
    
    strlcpy(data.tag_uid, payload["tag_uid"] | "", sizeof(data.tag_uid));
    strlcpy(data.key, payload["key"] | "", sizeof(data.key));
    data.timeout_seconds = payload["timeout_seconds"] | 0;
    
    // Validate
    if (!isValidTagUID(data.tag_uid) || !isValidHexKey(data.key) || 
        data.timeout_seconds < 1 || data.timeout_seconds > 300) {
        return false;
    }
    
    return true;
}

// Deserialize Auth Start Command
bool deserializeAuthStart(JsonObject payload, AuthStartPayload& data) {
    data.clear();
    
    if (!payload.containsKey("timeout_seconds")) {
        return false;
    }
    
    data.timeout_seconds = payload["timeout_seconds"] | 0;
    
    if (data.timeout_seconds < 1 || data.timeout_seconds > 300) {
        return false;
    }
    
    return true;
}

// Deserialize Auth Verify Command
bool deserializeAuthVerify(JsonObject payload, AuthVerifyPayload& data) {
    data.clear();
    
    if (!payload.containsKey("tag_uid") || !payload.containsKey("key") || 
        !payload.containsKey("user_data")) {
        return false;
    }
    
    strlcpy(data.tag_uid, payload["tag_uid"] | "", sizeof(data.tag_uid));
    strlcpy(data.key, payload["key"] | "", sizeof(data.key));
    
    JsonObject userData = payload["user_data"].as<JsonObject>();
    if (!deserializeUserData(userData, data.user_data)) {
        return false;
    }
    
    // Validate
    if (!isValidTagUID(data.tag_uid) || !isValidHexKey(data.key)) {
        return false;
    }
    
    return true;
}

// Deserialize Read Start Command
bool deserializeReadStart(JsonObject payload, ReadStartPayload& data) {
    data.clear();
    
    if (!payload.containsKey("timeout_seconds") || !payload.containsKey("read_blocks")) {
        return false;
    }
    
    data.timeout_seconds = payload["timeout_seconds"] | 0;
    
    JsonArray blocks = payload["read_blocks"].as<JsonArray>();
    data.read_blocks_count = 0;
    for (JsonVariant v : blocks) {
        if (data.read_blocks_count >= 256) break;
        data.read_blocks[data.read_blocks_count++] = v.as<uint8_t>();
    }
    
    if (data.timeout_seconds < 1 || data.timeout_seconds > 300 || data.read_blocks_count == 0) {
        return false;
    }
    
    return true;
}

// Serialize Status Change Event
bool serializeStatusChange(JsonObject payload, const StatusChangePayload& data) {
    payload["status"] = deviceStatusToString(data.status);
    payload["firmware_version"] = data.firmware_version;
    payload["ip_address"] = data.ip_address;
    return true;
}

// Serialize Mode Change Event
bool serializeModeChange(JsonObject payload, const ModeChangePayload& data) {
    payload["mode"] = deviceModeToString(data.mode);
    payload["previous_mode"] = deviceModeToString(data.previous_mode);
    return true;
}

// Serialize Tag Detected Event
bool serializeTagDetected(JsonObject payload, const TagDetectedPayload& data) {
    payload["tag_uid"] = data.tag_uid;
    payload["message"] = data.message;
    return true;
}

// Serialize Register Success Event
bool serializeRegisterSuccess(JsonObject payload, const RegisterSuccessPayload& data) {
    payload["tag_uid"] = data.tag_uid;
    payload["blocks_written"] = data.blocks_written;
    payload["message"] = data.message;
    return true;
}

// Serialize Auth Success Event
bool serializeAuthSuccess(JsonObject payload, const AuthSuccessPayload& data) {
    payload["tag_uid"] = data.tag_uid;
    payload["authenticated"] = data.authenticated;
    payload["message"] = data.message;
    
    JsonObject userData = payload.createNestedObject("user_data");
    serializeUserData(userData, data.user_data);
    
    return true;
}

// Serialize Auth Failed Event
bool serializeAuthFailed(JsonObject payload, const AuthFailedPayload& data) {
    payload["tag_uid"] = data.tag_uid;
    payload["authenticated"] = data.authenticated;
    payload["reason"] = data.reason;
    return true;
}

// Serialize Error Event
bool serializeError(JsonObject payload, const ErrorPayload& data) {
    payload["error"] = data.error;
    payload["error_code"] = errorCodeToString(data.error_code);
    payload["retry_possible"] = data.retry_possible;
    payload["component"] = errorComponentToString(data.component);
    return true;
}

// Serialize Heartbeat Event
bool serializeHeartbeat(JsonObject payload, const HeartbeatPayload& data) {
    payload["uptime_seconds"] = data.uptime_seconds;
    payload["memory_usage_percent"] = data.memory_usage_percent;
    payload["operations_completed"] = data.operations_completed;
    return true;
}

// Serialize Read Success Event
bool serializeReadSuccess(JsonObject payload, const ReadSuccessPayload& data) {
    payload["tag_uid"] = data.tag_uid;
    payload["message"] = data.message;
    
    JsonArray blocks = payload.createNestedArray("blocks_read");
    for (size_t i = 0; i < data.blocks_read_count; i++) {
        blocks.add(data.blocks_read[i]);
    }
    
    return true;
}

// Serialize User Data helper
bool serializeUserData(JsonObject obj, const UserData& userData) {
    if (strlen(userData.username) > 0) {
        obj["username"] = userData.username;
    }
    if (strlen(userData.context) > 0) {
        obj["context"] = userData.context;
    }
    return true;
}

// Deserialize User Data helper
bool deserializeUserData(JsonObject obj, UserData& userData) {
    userData.clear();
    
    if (obj.containsKey("username")) {
        strlcpy(userData.username, obj["username"] | "", sizeof(userData.username));
    }
    if (obj.containsKey("context")) {
        strlcpy(userData.context, obj["context"] | "", sizeof(userData.context));
    }
    
    return true;
}

// Validation: Tag UID (colon-separated hex format)
bool isValidTagUID(const char* uid) {
    if (uid == nullptr || strlen(uid) == 0) return false;
    
    // Must contain at least one colon
    if (strchr(uid, ':') == nullptr) return false;
    
    // Check format: pairs of hex digits separated by colons
    size_t len = strlen(uid);
    for (size_t i = 0; i < len; i++) {
        char c = uid[i];
        if (c == ':') continue;
        if (!isxdigit(c)) return false;
    }
    
    return true;
}

// Validation: Hex key (32 hex characters)
bool isValidHexKey(const char* key) {
    if (key == nullptr) return false;
    
    size_t len = strlen(key);
    if (len != 32) return false;
    
    for (size_t i = 0; i < len; i++) {
        if (!isxdigit(key[i])) return false;
    }
    
    return true;
}

// Validation: UUID v4
bool isValidUUID(const char* uuid) {
    if (uuid == nullptr) return false;
    
    size_t len = strlen(uuid);
    if (len != 36) return false;
    
    // Format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    // Check hyphens at positions 8, 13, 18, 23
    if (uuid[8] != '-' || uuid[13] != '-' || uuid[18] != '-' || uuid[23] != '-') {
        return false;
    }
    
    // Check all other characters are hex digits
    for (size_t i = 0; i < len; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) continue;
        if (!isxdigit(uuid[i])) return false;
    }
    
    return true;
}

// Validation: Device ID
bool isValidDeviceId(const char* deviceId) {
    if (deviceId == nullptr || strlen(deviceId) == 0) return false;
    
    size_t len = strlen(deviceId);
    if (len > MAX_DEVICE_ID_LENGTH) return false;
    
    // Must contain only alphanumeric, underscore, or hyphen
    for (size_t i = 0; i < len; i++) {
        char c = deviceId[i];
        if (!isalnum(c) && c != '_' && c != '-') return false;
    }
    
    return true;
}
