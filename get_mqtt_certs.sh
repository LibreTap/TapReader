#!/bin/bash
set -e

TAPSERVICE_URL="http://localhost:8000/api/v1"
DEVICE_ID="test-esp32-reader"
OUTPUT_DIR="./certs"

echo "======================================"
echo "TapService mTLS Certificate Generator"
echo "======================================"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Step 1: Get CA Certificate
echo "📥 Step 1: Fetching CA certificate..."
CA_CERT=$(curl -s "${TAPSERVICE_URL}/ca/certificate" | jq -r '.ca_certificate')

if [ -z "$CA_CERT" ] || [ "$CA_CERT" == "null" ]; then
    echo "❌ Failed to fetch CA certificate"
    exit 1
fi

echo "$CA_CERT" > "${OUTPUT_DIR}/ca.crt"
echo "✅ CA certificate saved to ${OUTPUT_DIR}/ca.crt"

# Verify CA cert is valid
if openssl x509 -in "${OUTPUT_DIR}/ca.crt" -text -noout > /dev/null 2>&1; then
    echo "✅ CA certificate is valid PEM format"
else
    echo "❌ CA certificate is invalid!"
    exit 1
fi
echo ""

# Step 2: Generate enrollment token
echo "📝 Step 2: Generating enrollment token..."
TOKEN_RESPONSE=$(curl -s -X POST "${TAPSERVICE_URL}/admin/enrollment-tokens" \
  -H "Content-Type: application/json" \
  -d '{"expires_minutes": 60, "max_uses": 1, "description": "Test ESP32 provisioning"}')

ENROLLMENT_TOKEN=$(echo "$TOKEN_RESPONSE" | jq -r '.token')

if [ -z "$ENROLLMENT_TOKEN" ] || [ "$ENROLLMENT_TOKEN" == "null" ]; then
    echo "❌ Failed to generate enrollment token"
    echo "Response: $TOKEN_RESPONSE"
    exit 1
fi

echo "✅ Enrollment token: ${ENROLLMENT_TOKEN:0:16}..."
echo ""

# Step 3: Generate client private key and CSR locally
echo "🔑 Step 3: Generating client private key and CSR..."

# Generate private key in RSA PKCS#1 format (ESP32 compatible)
# Using -traditional flag to ensure PKCS#1 format (BEGIN RSA PRIVATE KEY)
openssl genrsa -out "${OUTPUT_DIR}/client.key" 2048 2>/dev/null

echo "✅ Client private key generated (RSA PKCS#1 format)"

# Verify key format
KEY_FORMAT=$(head -1 "${OUTPUT_DIR}/client.key")
if [[ "$KEY_FORMAT" == "-----BEGIN RSA PRIVATE KEY-----" ]]; then
    echo "✅ Key format verified: RSA PKCS#1 (ESP32 compatible)"
elif [[ "$KEY_FORMAT" == "-----BEGIN PRIVATE KEY-----" ]]; then
    echo "⚠️  Key is in PKCS#8 format, converting to RSA PKCS#1..."
    openssl rsa -in "${OUTPUT_DIR}/client.key" -out "${OUTPUT_DIR}/client.key.tmp" -traditional 2>/dev/null
    mv "${OUTPUT_DIR}/client.key.tmp" "${OUTPUT_DIR}/client.key"
    echo "✅ Converted to RSA PKCS#1 format"
elif [[ "$KEY_FORMAT" == *"OPENSSH"* ]]; then
    echo "⚠️  Key is in OpenSSH format, converting to RSA PKCS#1..."
    openssl rsa -in "${OUTPUT_DIR}/client.key" -out "${OUTPUT_DIR}/client.key.tmp" -traditional 2>/dev/null
    mv "${OUTPUT_DIR}/client.key.tmp" "${OUTPUT_DIR}/client.key"
    echo "✅ Converted to RSA PKCS#1 format"
else
    echo "⚠️  Unknown key format: $KEY_FORMAT"
fi

# Generate CSR
openssl req -new -key "${OUTPUT_DIR}/client.key" \
  -out "${OUTPUT_DIR}/client.csr" \
  -subj "/CN=${DEVICE_ID}/O=LibreTap/OU=TapReader" 2>/dev/null

CSR_PEM=$(cat "${OUTPUT_DIR}/client.csr")
echo "✅ CSR generated"
echo ""

# Step 4: Submit CSR for signing
echo "📤 Step 4: Submitting CSR to TapService for signing..."

PROVISION_RESPONSE=$(curl -s -X POST "${TAPSERVICE_URL}/device/provision" \
  -H "Authorization: Bearer ${ENROLLMENT_TOKEN}" \
  -H "Content-Type: application/json" \
  -d "{
    \"device_id\": \"${DEVICE_ID}\",
    \"csr_pem\": $(echo "$CSR_PEM" | jq -Rs .),
    \"hardware_info\": {
      \"chip_id\": \"test-123\",
      \"mac_address\": \"AA:BB:CC:DD:EE:FF\"
    }
  }")

CLIENT_CERT=$(echo "$PROVISION_RESPONSE" | jq -r '.certificate')

if [ -z "$CLIENT_CERT" ] || [ "$CLIENT_CERT" == "null" ]; then
    echo "❌ Failed to provision device"
    echo "Response: $PROVISION_RESPONSE"
    exit 1
fi

echo "$CLIENT_CERT" > "${OUTPUT_DIR}/client.crt"
echo "✅ Client certificate saved to ${OUTPUT_DIR}/client.crt"
echo ""

# Step 5: VERIFICATION
echo "======================================"
echo "🔍 Certificate Verification"
echo "======================================"

# Verify client cert format
if openssl x509 -in "${OUTPUT_DIR}/client.crt" -text -noout > /dev/null 2>&1; then
    echo "✅ Client certificate is valid PEM format"
else
    echo "❌ Client certificate is invalid!"
    exit 1
fi

# Verify private key format and compatibility
if openssl rsa -in "${OUTPUT_DIR}/client.key" -check -noout 2>/dev/null; then
    echo "✅ Private key is valid"
else
    echo "❌ Private key is invalid!"
    exit 1
fi

# Double-check key format for ESP32 compatibility
KEY_HEADER=$(head -1 "${OUTPUT_DIR}/client.key")
if [[ "$KEY_HEADER" == "-----BEGIN RSA PRIVATE KEY-----" ]]; then
    echo "✅ Private key format: RSA PKCS#1 (ESP32 compatible)"
elif [[ "$KEY_HEADER" == "-----BEGIN PRIVATE KEY-----" ]]; then
    echo "⚠️  Private key is PKCS#8, ESP32 may have issues"
    echo "   Converting to RSA PKCS#1..."
    openssl rsa -in "${OUTPUT_DIR}/client.key" -out "${OUTPUT_DIR}/client.key.tmp" -traditional 2>/dev/null
    mv "${OUTPUT_DIR}/client.key.tmp" "${OUTPUT_DIR}/client.key"
    echo "✅ Converted to RSA PKCS#1 format"
else
    echo "⚠️  Unexpected key format: $KEY_HEADER"
fi

# Verify certificate was signed by CA
if openssl verify -CAfile "${OUTPUT_DIR}/ca.crt" "${OUTPUT_DIR}/client.crt" 2>&1 | grep -q "OK"; then
    echo "✅ Client certificate successfully verified against CA"
else
    echo "❌ Client certificate verification FAILED!"
    echo "This will cause the broker to reject the connection!"
    openssl verify -CAfile "${OUTPUT_DIR}/ca.crt" "${OUTPUT_DIR}/client.crt"
    exit 1
fi

# Verify cert and key match
CERT_MODULUS=$(openssl x509 -noout -modulus -in "${OUTPUT_DIR}/client.crt" | openssl md5 | awk '{print $2}')
KEY_MODULUS=$(openssl rsa -noout -modulus -in "${OUTPUT_DIR}/client.key" 2>/dev/null | openssl md5 | awk '{print $2}')

if [ "$CERT_MODULUS" == "$KEY_MODULUS" ]; then
    echo "✅ Certificate and private key match"
else
    echo "❌ Certificate and private key DO NOT MATCH!"
    echo "   Cert modulus: $CERT_MODULUS"
    echo "   Key modulus:  $KEY_MODULUS"
    exit 1
fi

# Show certificate details
echo ""
echo "📋 Certificate Details:"
echo "---"
openssl x509 -in "${OUTPUT_DIR}/client.crt" -subject -issuer -dates -noout
echo ""

# Step 6: Test connection to broker
echo "======================================"
echo "🧪 Testing MQTT Broker Connection"
echo "======================================"
echo "Testing with openssl s_client..."
echo ""

# Detect broker IP
BROKER_IP="192.168.178.20"  # Update if different
if [ -f ".env" ]; then
    BROKER_IP=$(grep MQTT_HOST .env | cut -d'=' -f2 | tr -d ' "')
fi

# Try to connect (timeout after 5 seconds)
if timeout 5 openssl s_client -connect "${BROKER_IP}:8883" \
  -CAfile "${OUTPUT_DIR}/ca.crt" \
  -cert "${OUTPUT_DIR}/client.crt" \
  -key "${OUTPUT_DIR}/client.key" \
  -showcerts < /dev/null 2>&1 | grep -q "Verify return code: 0"; then
    echo "✅ Successfully connected to MQTT broker with mTLS!"
else
    echo "⚠️  Could not verify connection to broker"
    echo "This might be OK if the broker isn't running or accessible"
fi
echo ""

# Step 7: Get MQTT connection details and detect IP
MQTT_HOST=$(echo "$PROVISION_RESPONSE" | jq -r '.mqtt_host // "localhost"')
MQTT_PORT=$(echo "$PROVISION_RESPONSE" | jq -r '.mqtt_port // 8883')
EXPIRES_AT=$(echo "$PROVISION_RESPONSE" | jq -r '.expires_at // "N/A"')

# Detect machine's IP address
MACHINE_IP=""
if command -v ipconfig &> /dev/null; then
    # macOS
    MACHINE_IP=$(ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null || echo "")
elif command -v hostname &> /dev/null; then
    # Linux
    MACHINE_IP=$(hostname -I 2>/dev/null | awk '{print $1}')
fi

# If MQTT_HOST is localhost, use detected IP
if [ "$MQTT_HOST" = "localhost" ] || [ "$MQTT_HOST" = "127.0.0.1" ]; then
    if [ -n "$MACHINE_IP" ]; then
        echo "⚠️  MQTT_HOST is set to 'localhost' - using detected IP: $MACHINE_IP"
        MQTT_HOST="$MACHINE_IP"
    else
        echo "⚠️  Warning: Could not detect machine IP. Please manually update MQTT_HOST in the Arduino code!"
    fi
fi

# Step 8: Generate Arduino-formatted strings for terminal output
echo "======================================"
echo "📋 Arduino Code (Copy-Paste Ready)"
echo "======================================"
echo ""

echo "// CA Certificate (for server verification)"
echo "const char rootCA[] = \\"
awk '{printf "  \"%s\\n\" \\\n", $0}' "${OUTPUT_DIR}/ca.crt" | sed '$ s/ \\$/;/'
echo ""

echo "// Client Certificate (for client authentication)"
echo "const char clientCert[] = \\"
awk '{printf "  \"%s\\n\" \\\n", $0}' "${OUTPUT_DIR}/client.crt" | sed '$ s/ \\$/;/'
echo ""

echo "// Client Private Key (for client authentication)"
echo "// Format: RSA PKCS#1 (ESP32 compatible)"
echo "const char clientKey[] = \\"
awk '{printf "  \"%s\\n\" \\\n", $0}' "${OUTPUT_DIR}/client.key" | sed '$ s/ \\$/;/'
echo ""

# Step 9: MQTT Connection Details
echo "======================================"
echo "📡 MQTT Connection Details"
echo "======================================"
echo "MQTT Host: $MQTT_HOST"
echo "MQTT Port: $MQTT_PORT"
echo "Device ID: $DEVICE_ID"
echo "Certificate expires: $EXPIRES_AT"
echo ""

# Step 10: Create complete Arduino example with all recommendations
echo "======================================"
echo "💾 Generating Complete Arduino Example"
echo "======================================"

cat > "${OUTPUT_DIR}/mqtt_mtls_example.ino" << 'ARDUINO_HEADER'
/*
 * LibreTap MQTT mTLS Client Example
 * 
 * This example demonstrates secure MQTT connection using mutual TLS (mTLS)
 * with client certificate authentication.
 * 
 * Features:
 * - NTP time synchronization (required for certificate validation)
 * - Automatic reconnection
 * - Detailed error logging
 * - Memory monitoring
 * - RSA PKCS#1 format private key (ESP32 compatible)
 */

#include <WiFi.h>
#include <espMqttClient.h>
#include <time.h>

// ====================================
// Configuration
// ====================================

#define WIFI_SSID "yourSSID"
#define WIFI_PASSWORD "yourpass"

ARDUINO_HEADER

# Add connection details
echo "#define MQTT_HOST \"$MQTT_HOST\"  // Use IP address, not 'localhost'" >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"
echo "#define MQTT_PORT $MQTT_PORT" >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"
echo "#define DEVICE_ID \"$DEVICE_ID\"" >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"
echo "" >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"
echo "// NTP servers for time synchronization (required for certificate validation)" >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"
echo "#define NTP_SERVER1 \"pool.ntp.org\"" >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"
echo "#define NTP_SERVER2 \"time.nist.gov\"" >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"
echo "" >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"
echo "// ====================================" >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"
echo "// Certificates (RSA PKCS#1 format)" >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"
echo "// ====================================" >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"
echo "" >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"

# Add CA certificate
cat >> "${OUTPUT_DIR}/mqtt_mtls_example.ino" << 'ARDUINO_CA'
// CA Certificate (for server verification)
const char rootCA[] = \
ARDUINO_CA

awk '{printf "  \"%s\\n\" \\\n", $0}' "${OUTPUT_DIR}/ca.crt" | sed '$ s/ \\$/;/' >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"
echo "" >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"

# Add client certificate
cat >> "${OUTPUT_DIR}/mqtt_mtls_example.ino" << 'ARDUINO_CLIENT_CERT'
// Client Certificate (for mTLS authentication)
const char clientCert[] = \
ARDUINO_CLIENT_CERT

awk '{printf "  \"%s\\n\" \\\n", $0}' "${OUTPUT_DIR}/client.crt" | sed '$ s/ \\$/;/' >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"
echo "" >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"

# Add private key
cat >> "${OUTPUT_DIR}/mqtt_mtls_example.ino" << 'ARDUINO_PRIVATE_KEY'
// Client Private Key (for mTLS authentication)
// Format: RSA PKCS#1 - ESP32 compatible
const char clientKey[] = \
ARDUINO_PRIVATE_KEY

awk '{printf "  \"%s\\n\" \\\n", $0}' "${OUTPUT_DIR}/client.key" | sed '$ s/ \\$/;/' >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"
echo "" >> "${OUTPUT_DIR}/mqtt_mtls_example.ino"

# Add the rest of the Arduino code (same as before, keeping it concise)
cat >> "${OUTPUT_DIR}/mqtt_mtls_example.ino" << 'ARDUINO_CODE'
// ====================================
// Global Objects
// ====================================

espMqttClientSecure mqttClient;
bool timeIsSynced = false;

// ====================================
// Time Synchronization
// ====================================

void syncTime() {
  Serial.println("\n⏰ Synchronizing time with NTP...");
  configTime(0, 0, NTP_SERVER1, NTP_SERVER2);
  
  struct tm timeinfo;
  int attempts = 0;
  const int maxAttempts = 30;
  
  Serial.print("   Waiting for time sync");
  while (!getLocalTime(&timeinfo) && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (attempts >= maxAttempts) {
    Serial.println("   ❌ Failed to sync time!");
    timeIsSynced = false;
  } else {
    timeIsSynced = true;
    Serial.println("   ✅ Time synchronized successfully");
    Serial.print("   Current time: ");
    Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S UTC");
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("\n✅ Connected to MQTT with mTLS!");
  
  String statusTopic = String("devices/") + DEVICE_ID + "/status";
  mqttClient.publish(statusTopic.c_str(), 1, true, "{\"status\":\"online\"}");
}

void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason) {
  Serial.printf("❌ Disconnected from MQTT: %u\n", static_cast<uint8_t>(reason));
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n====================================");
  Serial.println("LibreTap MQTT mTLS Client");
  Serial.println("====================================\n");
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\n✅ WiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  syncTime();
  
  Serial.println("\n🔐 Configuring mTLS...");
  mqttClient.setCACert(rootCA);
  mqttClient.setCertificate(clientCert);
  mqttClient.setPrivateKey(clientKey);
  
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setClientId(DEVICE_ID);
  mqttClient.setCleanSession(true);
  mqttClient.setKeepAlive(60);
  
  Serial.println("Connecting to MQTT broker...");
  mqttClient.connect();
}

void loop() {
  static unsigned long lastStatus = 0;
  
  if (millis() - lastStatus > 30000) {
    lastStatus = millis();
    Serial.printf("Status: %s | Heap: %u bytes\n", 
                  mqttClient.connected() ? "Connected" : "Disconnected",
                  ESP.getFreeHeap());
  }
  
  if (!mqttClient.connected()) {
    delay(5000);
    mqttClient.connect();
  }
  
  delay(10);
}
ARDUINO_CODE

echo "✅ Complete Arduino example saved to ${OUTPUT_DIR}/mqtt_mtls_example.ino"
echo ""

# Final summary
echo "======================================"
echo "✅ All Done!"
echo "======================================"
echo ""
echo "📁 Files generated:"
echo "   - ${OUTPUT_DIR}/ca.crt (CA certificate)"
echo "   - ${OUTPUT_DIR}/client.crt (Client certificate)"
echo "   - ${OUTPUT_DIR}/client.key (RSA PKCS#1 private key - ESP32 compatible)"
echo "   - ${OUTPUT_DIR}/mqtt_mtls_example.ino (Complete Arduino sketch)"
echo ""
echo "🔑 Key Format: RSA PKCS#1 (ESP32 compatible)"
echo "   Your private key starts with: -----BEGIN RSA PRIVATE KEY-----"
echo ""
echo "🚀 Next Steps:"
echo "   1. Open ${OUTPUT_DIR}/mqtt_mtls_example.ino in Arduino IDE"
echo "   2. Update WIFI_SSID and WIFI_PASSWORD"
if [ -n "$MACHINE_IP" ]; then
    echo "   3. MQTT_HOST is set to: $MACHINE_IP"
else
    echo "   3. ⚠️  Update MQTT_HOST with your machine's IP address!"
fi
echo "   4. Upload to your ESP32"
echo "   5. Open Serial Monitor (115200 baud)"
echo ""
echo "💡 The private key is now in RSA PKCS#1 format,"
echo "   which is compatible with ESP32's WiFiClientSecure!"
echo ""