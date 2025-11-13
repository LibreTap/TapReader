#include <unity.h>
#include <Arduino.h>
#include "mqtt_serialization.h"
#include "mqtt_types.h"

// =============================================================================
// TEST: Basic Round-Trip Tests (for embedded target)
// =============================================================================

void test_envelope_round_trip() {
    MQTTMessageEnvelope env1;
    strcpy(env1.version, "1.0");
    strcpy(env1.timestamp, "2025-11-13T12:00:00.000Z");
    strcpy(env1.device_id, "test-device-001");
    strcpy(env1.event_type, "status_change");
    strcpy(env1.request_id, "550e8400-e29b-41d4-a716-446655440000");
    
    char jsonBuffer[512];
    bool result = serializeEnvelope(env1, jsonBuffer, sizeof(jsonBuffer));
    TEST_ASSERT_TRUE(result);
    
    MQTTMessageEnvelope env2;
    StaticJsonDocument<MQTT_ENVELOPE_DOC_SIZE> doc;
    result = deserializeEnvelope(jsonBuffer, env2, doc);
    TEST_ASSERT_TRUE(result);
    
    TEST_ASSERT_EQUAL_STRING(env1.version, env2.version);
    TEST_ASSERT_EQUAL_STRING(env1.device_id, env2.device_id);
}

void test_status_change_serialization() {
    StatusChangePayload payload;
    payload.status = DeviceStatus::ONLINE;
    strcpy(payload.firmware_version, "1.2.3");
    strcpy(payload.ip_address, "192.168.1.100");
    
    StaticJsonDocument<MQTT_EVENT_DOC_SIZE> doc;
    JsonObject obj = doc.to<JsonObject>();
    
    bool result = serializeStatusChange(obj, payload);
    TEST_ASSERT_TRUE(result);
    
    TEST_ASSERT_EQUAL_STRING("online", obj["status"]);
    TEST_ASSERT_EQUAL_STRING("1.2.3", obj["firmware_version"]);
}

void test_register_start_deserialization() {
    const char* json = R"({
        "tag_uid": "04:A1:B2:C3:D4:E5:F6",
        "key": "0123456789ABCDEF0123456789ABCDEF",
        "timeout_seconds": 30
    })";
    
    StaticJsonDocument<MQTT_COMMAND_DOC_SIZE> doc;
    deserializeJson(doc, json);
    JsonObject payload = doc.as<JsonObject>();
    
    RegisterStartPayload data;
    bool result = deserializeRegisterStart(payload, data);
    TEST_ASSERT_TRUE(result);
    
    TEST_ASSERT_EQUAL(30, data.timeout_seconds);
}

void test_validation_functions() {
    TEST_ASSERT_TRUE(isValidTagUID("04:A1:B2:C3"));
    TEST_ASSERT_FALSE(isValidTagUID("04A1B2C3"));
    
    TEST_ASSERT_TRUE(isValidHexKey("0123456789ABCDEF0123456789ABCDEF"));
    TEST_ASSERT_FALSE(isValidHexKey("0123456789ABCDEF"));
    
    TEST_ASSERT_TRUE(isValidUUID("550e8400-e29b-41d4-a716-446655440000"));
}

void setup() {
    delay(2000);
    
    UNITY_BEGIN();
    
    RUN_TEST(test_envelope_round_trip);
    RUN_TEST(test_status_change_serialization);
    RUN_TEST(test_register_start_deserialization);
    RUN_TEST(test_validation_functions);
    
    UNITY_END();
}

void loop() {
    // Empty
}
