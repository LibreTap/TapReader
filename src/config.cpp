#include "config.h"

Preferences preferences;
WiFiManager wm;

void run_config_portal()
{
    // reset settings - for testing
    // wm.resetSettings();

    // set configportal timeout
    wm.setConfigPortalTimeout(240);
    wm.setConnectTimeout(20);
    WiFiManagerParameter mqtturl("mqtturl", "Ip adress", "mqtt.olga-tech.de", 50);
    WiFiManagerParameter mqttusr("mqttusr", "MQTT User", "testuser2", 50);
    WiFiManagerParameter mqttpw("mqttpw", "MQTT PW", "testuser2", 50);
    WiFiManagerParameter mqttid("mqttid", "MQTT ID", "TestClient", 50);
    WiFiManagerParameter mqttport("mqttport", "MQTT_PORT", "1883", 50);

    wm.addParameter(&mqtturl);
    wm.addParameter(&mqttusr);
    wm.addParameter(&mqttpw);
    wm.addParameter(&mqttid);
    wm.addParameter(&mqttport);

    if (!wm.startConfigPortal("Emma-Terminal"))
    {
        Serial.println("failed to connect and hit timeout");
        // reset and try again, or maybe put it to deep slee
    }

    else
    {
        // if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");
        preferences.putString("mqttssid", wm.getWiFiSSID(true));
        preferences.putString("mqttkey", wm.getWiFiPass(true));
    }

    delay(1000);

    Serial.println(mqtturl.getValue());
    Serial.println(mqttusr.getValue());
    Serial.println(mqttpw.getValue());
    Serial.println(mqttid.getValue());
    Serial.println(mqttid.getValue());

    preferences.putString("mqtturl", mqtturl.getValue());
    preferences.putString("mqttusr", mqttusr.getValue());
    preferences.putString("mqttpw", mqttpw.getValue());
    preferences.putString("mqttid", mqttid.getValue());
    preferences.putString("mqttport", mqttport.getValue());

    preferences.end();

    delay(1000);

    ESP.restart();
}