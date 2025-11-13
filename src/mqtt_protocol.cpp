#include "mqtt_protocol.h"

// ===== MQTTMessageBuilder Implementation =====

MQTTMessageBuilder::MQTTMessageBuilder() {
    memset(deviceId, 0, sizeof(deviceId));
    memset(buffer, 0, sizeof(buffer));
}

void MQTTMessageBuilder::setDeviceId(const char* id) {
    strlcpy(deviceId, id, sizeof(deviceId));
}

const char* MQTTMessageBuilder::buildMessage(EventType eventType, const char* requestId,
                                             bool (*serializePayload)(JsonObject, const void*),
                                             const void* payloadData) {
    doc.clear();
    
    // Generate timestamp
    char timestamp[32];
    generateTimestamp(timestamp, sizeof(timestamp));
    
    // Build envelope
    doc["version"] = MQTT_PROTOCOL_VERSION;
    doc["timestamp"] = timestamp;
    doc["device_id"] = deviceId;
    doc["event_type"] = eventTypeToString(eventType);
    doc["request_id"] = requestId;
    
    // Serialize payload
    JsonObject payload = doc.createNestedObject("payload");
    if (payloadData != nullptr && serializePayload != nullptr) {
        serializePayload(payload, payloadData);
    }
    
    // Serialize to buffer
    memset(buffer, 0, sizeof(buffer));
    size_t size = serializeJson(doc, buffer, sizeof(buffer));
    
    if (size == 0) {
        Serial.println(F("Failed to serialize message"));
        return nullptr;
    }
    
    return buffer;
}

// Wrapper functions for serialization
static bool serializeStatusChangeWrapper(JsonObject payload, const void* data) {
    return serializeStatusChange(payload, *static_cast<const StatusChangePayload*>(data));
}

static bool serializeModeChangeWrapper(JsonObject payload, const void* data) {
    return serializeModeChange(payload, *static_cast<const ModeChangePayload*>(data));
}

static bool serializeTagDetectedWrapper(JsonObject payload, const void* data) {
    return serializeTagDetected(payload, *static_cast<const TagDetectedPayload*>(data));
}

static bool serializeRegisterSuccessWrapper(JsonObject payload, const void* data) {
    return serializeRegisterSuccess(payload, *static_cast<const RegisterSuccessPayload*>(data));
}

static bool serializeAuthSuccessWrapper(JsonObject payload, const void* data) {
    return serializeAuthSuccess(payload, *static_cast<const AuthSuccessPayload*>(data));
}

static bool serializeAuthFailedWrapper(JsonObject payload, const void* data) {
    return serializeAuthFailed(payload, *static_cast<const AuthFailedPayload*>(data));
}

static bool serializeErrorWrapper(JsonObject payload, const void* data) {
    return serializeError(payload, *static_cast<const ErrorPayload*>(data));
}

static bool serializeHeartbeatWrapper(JsonObject payload, const void* data) {
    return serializeHeartbeat(payload, *static_cast<const HeartbeatPayload*>(data));
}

static bool serializeReadSuccessWrapper(JsonObject payload, const void* data) {
    return serializeReadSuccess(payload, *static_cast<const ReadSuccessPayload*>(data));
}

const char* MQTTMessageBuilder::buildStatusChange(const char* requestId, const StatusChangePayload& payload) {
    return buildMessage(EventType::STATUS_CHANGE, requestId, serializeStatusChangeWrapper, &payload);
}

const char* MQTTMessageBuilder::buildModeChange(const char* requestId, const ModeChangePayload& payload) {
    return buildMessage(EventType::MODE_CHANGE, requestId, serializeModeChangeWrapper, &payload);
}

const char* MQTTMessageBuilder::buildTagDetected(const char* requestId, const TagDetectedPayload& payload) {
    return buildMessage(EventType::AUTH_TAG_DETECTED, requestId, serializeTagDetectedWrapper, &payload);
}

const char* MQTTMessageBuilder::buildRegisterSuccess(const char* requestId, const RegisterSuccessPayload& payload) {
    return buildMessage(EventType::REGISTER_SUCCESS, requestId, serializeRegisterSuccessWrapper, &payload);
}

const char* MQTTMessageBuilder::buildRegisterError(const char* requestId, const ErrorPayload& payload) {
    return buildMessage(EventType::REGISTER_ERROR, requestId, serializeErrorWrapper, &payload);
}

const char* MQTTMessageBuilder::buildAuthSuccess(const char* requestId, const AuthSuccessPayload& payload) {
    return buildMessage(EventType::AUTH_SUCCESS, requestId, serializeAuthSuccessWrapper, &payload);
}

const char* MQTTMessageBuilder::buildAuthFailed(const char* requestId, const AuthFailedPayload& payload) {
    return buildMessage(EventType::AUTH_FAILED, requestId, serializeAuthFailedWrapper, &payload);
}

const char* MQTTMessageBuilder::buildAuthError(const char* requestId, const ErrorPayload& payload) {
    return buildMessage(EventType::AUTH_ERROR, requestId, serializeErrorWrapper, &payload);
}

const char* MQTTMessageBuilder::buildReadSuccess(const char* requestId, const ReadSuccessPayload& payload) {
    return buildMessage(EventType::READ_SUCCESS, requestId, serializeReadSuccessWrapper, &payload);
}

const char* MQTTMessageBuilder::buildReadError(const char* requestId, const ErrorPayload& payload) {
    return buildMessage(EventType::READ_ERROR, requestId, serializeErrorWrapper, &payload);
}

const char* MQTTMessageBuilder::buildHeartbeat(const char* requestId, const HeartbeatPayload& payload) {
    return buildMessage(EventType::HEARTBEAT, requestId, serializeHeartbeatWrapper, &payload);
}

// ===== MQTTMessageParser Implementation =====

MQTTMessageParser::MQTTMessageParser() {
    memset(&envelope, 0, sizeof(envelope));
}

bool MQTTMessageParser::parse(const char* jsonBuffer) {
    // Clear previous state
    doc.clear();
    memset(&envelope, 0, sizeof(envelope));
    
    // Parse JSON
    DeserializationError error = deserializeJson(doc, jsonBuffer);
    if (error) {
        Serial.print(F("MQTTMessageParser parse failed: "));
        Serial.println(error.c_str());
        return false;
    }
    
    // Validate and extract envelope
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
    
    // Validate protocol version
    if (strcmp(envelope.version, MQTT_PROTOCOL_VERSION) != 0) {
        Serial.print(F("Unsupported protocol version: "));
        Serial.println(envelope.version);
        return false;
    }
    
    return true;
}

CommandType MQTTMessageParser::getCommandType() const {
    return stringToCommandType(envelope.event_type);
}

bool MQTTMessageParser::parseRegisterStart(RegisterStartPayload& payload) {
    return deserializeRegisterStart(envelope.payload, payload);
}

bool MQTTMessageParser::parseAuthStart(AuthStartPayload& payload) {
    return deserializeAuthStart(envelope.payload, payload);
}

bool MQTTMessageParser::parseAuthVerify(AuthVerifyPayload& payload) {
    return deserializeAuthVerify(envelope.payload, payload);
}

bool MQTTMessageParser::parseReadStart(ReadStartPayload& payload) {
    return deserializeReadStart(envelope.payload, payload);
}

bool MQTTMessageParser::isCancel() const {
    CommandType type = getCommandType();
    return type == CommandType::REGISTER_CANCEL ||
           type == CommandType::AUTH_CANCEL ||
           type == CommandType::READ_CANCEL;
}

bool MQTTMessageParser::isReset() const {
    return getCommandType() == CommandType::RESET;
}

// ===== MQTTTopicBuilder Implementation =====

MQTTTopicBuilder::MQTTTopicBuilder() {
    memset(deviceId, 0, sizeof(deviceId));
    memset(topicBuffer, 0, sizeof(topicBuffer));
}

void MQTTTopicBuilder::setDeviceId(const char* id) {
    strlcpy(deviceId, id, sizeof(deviceId));
}

// Command topics (Subscribe)
const char* MQTTTopicBuilder::registerStart() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/register/start", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::registerCancel() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/register/cancel", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::authStart() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/auth/start", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::authVerify() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/auth/verify", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::authCancel() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/auth/cancel", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::readStart() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/read/start", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::readCancel() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/read/cancel", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::reset() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/reset", deviceId);
    return topicBuffer;
}

// Event topics (Publish)
const char* MQTTTopicBuilder::registerSuccess() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/register/success", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::registerError() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/register/error", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::authTagDetected() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/auth/tag_detected", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::authSuccess() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/auth/success", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::authFailed() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/auth/failed", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::authError() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/auth/error", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::readSuccess() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/read/success", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::readError() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/read/error", deviceId);
    return topicBuffer;
}

// State topics (Publish with retain)
const char* MQTTTopicBuilder::status() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/status", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::mode() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/mode", deviceId);
    return topicBuffer;
}

const char* MQTTTopicBuilder::heartbeat() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/heartbeat", deviceId);
    return topicBuffer;
}

// Wildcard subscription
const char* MQTTTopicBuilder::allCommands() {
    snprintf(topicBuffer, sizeof(topicBuffer), "devices/%s/#", deviceId);
    return topicBuffer;
}
