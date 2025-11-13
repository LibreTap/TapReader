#include <unity.h>

#ifdef UNIT_TEST
#include "arduino_mocks.h"
#endif

#include "../../include/mqtt_serialization.h"
#include "../../include/mqtt_types.h"

// =============================================================================
// TEST: Message Envelope Round-Trip
// =============================================================================

void test_envelope_round_trip() {
    // Create envelope
    MQTTMessageEnvelope env1;
    strcpy(env1.version, "1.0");
    strcpy(env1.timestamp, "2025-11-13T12:00:00.000Z");
    strcpy(env1.device_id, "test-device-001");
    strcpy(env1.event_type, "status_change");
    strcpy(env1.request_id, "550e8400-e29b-41d4-a716-446655440000");
    
    // Serialize
    char jsonBuffer[512];
    bool result = serializeEnvelope(env1, jsonBuffer, sizeof(jsonBuffer));
    TEST_ASSERT_TRUE_MESSAGE(result, "Failed to serialize envelope");
    TEST_ASSERT_TRUE(strlen(jsonBuffer) > 0);
    
    printf("Envelope JSON: %s\n", jsonBuffer);
    
    // Deserialize
    MQTTMessageEnvelope env2;
    StaticJsonDocument<MQTT_ENVELOPE_DOC_SIZE> doc;
    result = deserializeEnvelope(jsonBuffer, env2, doc);
    TEST_ASSERT_TRUE_MESSAGE(result, "Failed to deserialize envelope");
    
    // Verify round-trip
    TEST_ASSERT_EQUAL_STRING(env1.version, env2.version);
    TEST_ASSERT_EQUAL_STRING(env1.timestamp, env2.timestamp);
    TEST_ASSERT_EQUAL_STRING(env1.device_id, env2.device_id);
    TEST_ASSERT_EQUAL_STRING(env1.event_type, env2.event_type);
    TEST_ASSERT_EQUAL_STRING(env1.request_id, env2.request_id);
}

// =============================================================================
// TEST: Status Change Event Serialization
// =============================================================================

void test_status_change_serialization() {
    StatusChangePayload payload;
    payload.status = DeviceStatus::ONLINE;
    strcpy(payload.firmware_version, "1.2.3");
    strcpy(payload.ip_address, "192.168.1.100");
    
    StaticJsonDocument<MQTT_EVENT_DOC_SIZE> doc;
    JsonObject obj = doc.to<JsonObject>();
    
    bool result = serializeStatusChange(obj, payload);
    TEST_ASSERT_TRUE(result);
    
    // Verify JSON content
    TEST_ASSERT_EQUAL_STRING("online", obj["status"]);
    TEST_ASSERT_EQUAL_STRING("1.2.3", obj["firmware_version"]);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", obj["ip_address"]);
    
    // Serialize to string and verify it's valid JSON
    char jsonBuffer[256];
    size_t size = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    TEST_ASSERT_TRUE(size > 0);
    
    printf("Status Change: %s\n", jsonBuffer);
}

// =============================================================================
// TEST: Register Start Command Deserialization
// =============================================================================

void test_register_start_deserialization() {
    const char* json = R"({
        "tag_uid": "04:A1:B2:C3:D4:E5:F6",
        "key": "0123456789ABCDEF0123456789ABCDEF",
        "timeout_seconds": 30
    })";
    
    StaticJsonDocument<MQTT_COMMAND_DOC_SIZE> doc;
    DeserializationError error = deserializeJson(doc, json);
    TEST_ASSERT_FALSE(error);
    
    JsonObject payload = doc.as<JsonObject>();
    RegisterStartPayload data;
    
    bool result = deserializeRegisterStart(payload, data);
    TEST_ASSERT_TRUE_MESSAGE(result, "Failed to deserialize register_start");
    
    TEST_ASSERT_EQUAL_STRING("04:A1:B2:C3:D4:E5:F6", data.tag_uid);
    TEST_ASSERT_EQUAL_STRING("0123456789ABCDEF0123456789ABCDEF", data.key);
    TEST_ASSERT_EQUAL(30, data.timeout_seconds);
    
    printf("Register Start deserialized successfully\n");
}

// =============================================================================
// TEST: Auth Success Round Trip
// =============================================================================

void test_auth_success_round_trip() {
    // Create payload
    AuthSuccessPayload payload1;
    strcpy(payload1.tag_uid, "04:AA:BB:CC:DD:EE:FF");
    payload1.authenticated = true;
    strcpy(payload1.message, "Authentication successful");
    strcpy(payload1.user_data.username, "john_doe");
    strcpy(payload1.user_data.context, "main_door");
    
    // Serialize
    StaticJsonDocument<MQTT_EVENT_DOC_SIZE> doc;
    JsonObject obj = doc.to<JsonObject>();
    bool result = serializeAuthSuccess(obj, payload1);
    TEST_ASSERT_TRUE(result);
    
    // Convert to JSON string
    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    
    printf("Auth Success: %s\n", jsonBuffer);
    
    // Deserialize back (simulating what service would do)
    StaticJsonDocument<MQTT_EVENT_DOC_SIZE> doc2;
    deserializeJson(doc2, jsonBuffer);
    JsonObject obj2 = doc2.as<JsonObject>();
    
    // Verify data
    TEST_ASSERT_EQUAL_STRING(payload1.tag_uid, obj2["tag_uid"]);
    TEST_ASSERT_TRUE(obj2["authenticated"]);
    TEST_ASSERT_EQUAL_STRING(payload1.message, obj2["message"]);
    TEST_ASSERT_EQUAL_STRING(payload1.user_data.username, obj2["user_data"]["username"]);
    TEST_ASSERT_EQUAL_STRING(payload1.user_data.context, obj2["user_data"]["context"]);
}

// =============================================================================
// TEST: Validation Functions
// =============================================================================

void test_validation_functions() {
    // Valid tag UIDs
    TEST_ASSERT_TRUE(isValidTagUID("04:A1:B2:C3"));
    TEST_ASSERT_TRUE(isValidTagUID("04:AA:BB:CC:DD:EE:FF"));
    
    // Invalid tag UIDs
    TEST_ASSERT_FALSE(isValidTagUID(""));
    TEST_ASSERT_FALSE(isValidTagUID("04A1B2C3"));  // No colons
    TEST_ASSERT_FALSE(isValidTagUID("04:ZZ:11"));  // Invalid hex
    
    // Valid hex keys
    TEST_ASSERT_TRUE(isValidHexKey("0123456789ABCDEF0123456789ABCDEF"));
    TEST_ASSERT_TRUE(isValidHexKey("00000000000000000000000000000000"));
    
    // Invalid hex keys
    TEST_ASSERT_FALSE(isValidHexKey(""));
    TEST_ASSERT_FALSE(isValidHexKey("0123456789ABCDEF"));  // Too short
    TEST_ASSERT_FALSE(isValidHexKey("0123456789ABCDEF0123456789ABCDEG"));  // Invalid char
    
    // Valid UUIDs
    TEST_ASSERT_TRUE(isValidUUID("550e8400-e29b-41d4-a716-446655440000"));
    
    // Invalid UUIDs
    TEST_ASSERT_FALSE(isValidUUID(""));
    TEST_ASSERT_FALSE(isValidUUID("550e8400-e29b-41d4"));  // Too short
    TEST_ASSERT_FALSE(isValidUUID("550e8400-e29b-41d4-a716-44665544000G"));  // Invalid char
    
    // Valid device IDs
    TEST_ASSERT_TRUE(isValidDeviceId("device-001"));
    TEST_ASSERT_TRUE(isValidDeviceId("reader_main"));
    
    // Invalid device IDs
    TEST_ASSERT_FALSE(isValidDeviceId(""));
    TEST_ASSERT_FALSE(isValidDeviceId("device@001"));  // Invalid char
    
    printf("All validation tests passed\n");
}

// =============================================================================
// TEST: Error Payload Serialization
// =============================================================================

void test_error_payload() {
    ErrorPayload payload;
    strcpy(payload.error, "NFC timeout occurred");
    payload.error_code = ErrorCode::NFC_TIMEOUT;
    payload.retry_possible = true;
    payload.component = ErrorComponent::NFC;
    
    StaticJsonDocument<MQTT_EVENT_DOC_SIZE> doc;
    JsonObject obj = doc.to<JsonObject>();
    
    bool result = serializeError(obj, payload);
    TEST_ASSERT_TRUE(result);
    
    TEST_ASSERT_EQUAL_STRING("NFC timeout occurred", obj["error"]);
    TEST_ASSERT_EQUAL_STRING("NFC_TIMEOUT", obj["error_code"]);
    TEST_ASSERT_TRUE(obj["retry_possible"]);
    TEST_ASSERT_EQUAL_STRING("nfc", obj["component"]);
    
    char jsonBuffer[256];
    serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    printf("Error Payload: %s\n", jsonBuffer);
}

// =============================================================================
// TEST: Read Start Command with Multiple Blocks
// =============================================================================

void test_read_start_with_blocks() {
    const char* json = R"({
        "timeout_seconds": 45,
        "read_blocks": [4, 5, 6, 7, 8]
    })";
    
    StaticJsonDocument<MQTT_COMMAND_DOC_SIZE> doc;
    deserializeJson(doc, json);
    JsonObject payload = doc.as<JsonObject>();
    
    ReadStartPayload data;
    bool result = deserializeReadStart(payload, data);
    TEST_ASSERT_TRUE_MESSAGE(result, "Failed to deserialize read_start");
    
    TEST_ASSERT_EQUAL(45, data.timeout_seconds);
    TEST_ASSERT_EQUAL(5, data.read_blocks_count);
    TEST_ASSERT_EQUAL(4, data.read_blocks[0]);
    TEST_ASSERT_EQUAL(5, data.read_blocks[1]);
    TEST_ASSERT_EQUAL(6, data.read_blocks[2]);
    TEST_ASSERT_EQUAL(7, data.read_blocks[3]);
    TEST_ASSERT_EQUAL(8, data.read_blocks[4]);
    
    printf("Read Start with blocks deserialized successfully\n");
}

// =============================================================================
// TEST: Invalid Data Rejection
// =============================================================================

void test_invalid_data_rejection() {
    // Invalid timeout (too large)
    const char* json1 = R"({"timeout_seconds": 500})";
    StaticJsonDocument<MQTT_COMMAND_DOC_SIZE> doc1;
    deserializeJson(doc1, json1);
    AuthStartPayload data1;
    TEST_ASSERT_FALSE_MESSAGE(
        deserializeAuthStart(doc1.as<JsonObject>(), data1),
        "Should reject timeout > 300"
    );
    
    // Invalid timeout (too small)
    const char* json2 = R"({"timeout_seconds": 0})";
    StaticJsonDocument<MQTT_COMMAND_DOC_SIZE> doc2;
    deserializeJson(doc2, json2);
    AuthStartPayload data2;
    TEST_ASSERT_FALSE_MESSAGE(
        deserializeAuthStart(doc2.as<JsonObject>(), data2),
        "Should reject timeout < 1"
    );
    
    // Invalid hex key (wrong length)
    const char* json3 = R"({
        "tag_uid": "04:AA:BB:CC",
        "key": "0123456789ABCDEF",
        "timeout_seconds": 30
    })";
    StaticJsonDocument<MQTT_COMMAND_DOC_SIZE> doc3;
    deserializeJson(doc3, json3);
    RegisterStartPayload data3;
    TEST_ASSERT_FALSE_MESSAGE(
        deserializeRegisterStart(doc3.as<JsonObject>(), data3),
        "Should reject invalid key length"
    );
    
    printf("All invalid data rejection tests passed\n");
}

// =============================================================================
// TEST: Mode Change Event
// =============================================================================

void test_mode_change_serialization() {
    ModeChangePayload payload;
    payload.mode = DeviceMode::AUTH;
    payload.previous_mode = DeviceMode::IDLE;
    
    StaticJsonDocument<MQTT_EVENT_DOC_SIZE> doc;
    JsonObject obj = doc.to<JsonObject>();
    
    bool result = serializeModeChange(obj, payload);
    TEST_ASSERT_TRUE(result);
    
    TEST_ASSERT_EQUAL_STRING("auth", obj["mode"]);
    TEST_ASSERT_EQUAL_STRING("idle", obj["previous_mode"]);
    
    char jsonBuffer[256];
    serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    printf("Mode Change: %s\n", jsonBuffer);
}

// =============================================================================
// TEST: Heartbeat Event
// =============================================================================

void test_heartbeat_serialization() {
    HeartbeatPayload payload;
    payload.uptime_seconds = 3600;
    payload.memory_usage_percent = 45;
    payload.operations_completed = 42;
    
    StaticJsonDocument<MQTT_EVENT_DOC_SIZE> doc;
    JsonObject obj = doc.to<JsonObject>();
    
    bool result = serializeHeartbeat(obj, payload);
    TEST_ASSERT_TRUE(result);
    
    TEST_ASSERT_EQUAL(3600, obj["uptime_seconds"]);
    TEST_ASSERT_EQUAL(45, obj["memory_usage_percent"]);
    TEST_ASSERT_EQUAL(42, obj["operations_completed"]);
    
    char jsonBuffer[256];
    serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    printf("Heartbeat: %s\n", jsonBuffer);
}

// =============================================================================
// MAIN TEST SETUP
// =============================================================================

void setUp(void) {
    // Set up for each test
}

void tearDown(void) {
    // Clean up after each test
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_envelope_round_trip);
    RUN_TEST(test_status_change_serialization);
    RUN_TEST(test_register_start_deserialization);
    RUN_TEST(test_auth_success_round_trip);
    RUN_TEST(test_validation_functions);
    RUN_TEST(test_error_payload);
    RUN_TEST(test_read_start_with_blocks);
    RUN_TEST(test_invalid_data_rejection);
    RUN_TEST(test_mode_change_serialization);
    RUN_TEST(test_heartbeat_serialization);
    
    return UNITY_END();
}
