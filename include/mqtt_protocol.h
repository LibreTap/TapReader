#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "mqtt_schema.h"
#include "mqtt_types.h"
#include "mqtt_serialization.h"

// MQTT Message Builder - helps construct messages to send
class MQTTMessageBuilder {
private:
    char deviceId[MAX_DEVICE_ID_LENGTH + 1];
    StaticJsonDocument<MQTT_EVENT_DOC_SIZE> doc;
    char buffer[1024];
    
public:
    MQTTMessageBuilder();
    
    // Set the device ID for all messages
    void setDeviceId(const char* id);
    
    // Build event messages (Device → Service)
    const char* buildStatusChange(const char* requestId, const StatusChangePayload& payload);
    const char* buildModeChange(const char* requestId, const ModeChangePayload& payload);
    const char* buildTagDetected(const char* requestId, const TagDetectedPayload& payload);
    const char* buildRegisterSuccess(const char* requestId, const RegisterSuccessPayload& payload);
    const char* buildRegisterError(const char* requestId, const ErrorPayload& payload);
    const char* buildAuthSuccess(const char* requestId, const AuthSuccessPayload& payload);
    const char* buildAuthFailed(const char* requestId, const AuthFailedPayload& payload);
    const char* buildAuthError(const char* requestId, const ErrorPayload& payload);
    const char* buildReadSuccess(const char* requestId, const ReadSuccessPayload& payload);
    const char* buildReadError(const char* requestId, const ErrorPayload& payload);
    const char* buildHeartbeat(const char* requestId, const HeartbeatPayload& payload);
    
private:
    const char* buildMessage(EventType eventType, const char* requestId, 
                            bool (*serializePayload)(JsonObject, const void*), 
                            const void* payload);
};

// MQTT Message Parser - helps parse received command messages
class MQTTMessageParser {
private:
    StaticJsonDocument<MQTT_COMMAND_DOC_SIZE> doc;
    MQTTMessageEnvelope envelope;
    
public:
    MQTTMessageParser();
    
    // Parse a received message
    bool parse(const char* jsonBuffer);
    
    // Get the parsed envelope
    const MQTTMessageEnvelope& getEnvelope() const { return envelope; }
    
    // Get command type
    CommandType getCommandType() const;
    
    // Parse command payloads
    bool parseRegisterStart(RegisterStartPayload& payload);
    bool parseAuthStart(AuthStartPayload& payload);
    bool parseAuthVerify(AuthVerifyPayload& payload);
    bool parseReadStart(ReadStartPayload& payload);
    bool isCancel() const;
    bool isReset() const;
    
    // Get request ID for response correlation
    const char* getRequestId() const { return envelope.request_id; }
    const char* getDeviceId() const { return envelope.device_id; }
};

// Topic Builder - constructs MQTT topic strings
class MQTTTopicBuilder {
private:
    char deviceId[MAX_DEVICE_ID_LENGTH + 1];
    char topicBuffer[256];
    
public:
    MQTTTopicBuilder();
    
    void setDeviceId(const char* id);
    
    // Command topics (Subscribe - Service → Device)
    const char* registerStart();
    const char* registerCancel();
    const char* authStart();
    const char* authVerify();
    const char* authCancel();
    const char* readStart();
    const char* readCancel();
    const char* reset();
    
    // Event topics (Publish - Device → Service)
    const char* registerSuccess();
    const char* registerError();
    const char* authTagDetected();
    const char* authSuccess();
    const char* authFailed();
    const char* authError();
    const char* readSuccess();
    const char* readError();
    
    // State topics (Publish with retain - Device → Service)
    const char* status();
    const char* mode();
    const char* heartbeat();
    
    // Wildcard subscription helpers
    const char* allCommands();  // Subscribe to all command topics
};
