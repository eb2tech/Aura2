#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include "forecast_mqtt.h"
#include "forecast_preferences.h"

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
bool mqttConnected = false;

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    std::string message((char *)payload, length);

    Serial.print("Message arrived: ");
    Serial.print(message.c_str());
    Serial.print(" on topic: ");
    Serial.println(topic);

    // mqttDispatcher.dispatch(topic, message);
}

bool discoverMqttBroker()
{
    if (mqttServer != "")
    {
        Serial.println("MQTT server already configured: " + mqttServer);
        return true;
    }

    Serial.println("Discovering MQTT broker via mDNS...");
    int n = MDNS.queryService("mqtt", "tcp");
    if (n == 0)
    {
        Serial.println("No MQTT broker found via mDNS");
        return false;
    }
    else
    {
        mqttServer = MDNS.hostname(0);
        Serial.println("Discovered MQTT broker: " + mqttServer);
        return true;
    }
}

void checkMqttConnection(lv_timer_t *timer)
{
    if (!mqttClient.connected())
    {
        Serial.println("MQTT not connected, attempting to connect...");

        mqttClient.setServer(mqttServer.c_str(), 1883);
        mqttClient.setCallback(mqttCallback);

        Serial.println("MQTT client configured to connect to: " + mqttServer);

        try
        {
            // bool success = false;
            // if (mqttUser.isEmpty())
            // {
            //     success = mqttClient.connect(getDeviceIdentifier().c_str());
            // }
            // else
            // {
            //     success = mqttClient.connect(getDeviceIdentifier().c_str(), mqttUser.c_str(), mqttPassword.c_str());
            // }

            if (mqttClient.connect(getDeviceIdentifier().c_str(), mqttUser.c_str(), mqttPassword.c_str()))
            {
                Serial.println("MQTT connected successfully");
                mqttConnected = true;
                // Resubscribe or publish initial messages if needed
            }
            else
            {
                Serial.print("MQTT connection failed, rc=");
                Serial.print(mqttClient.state());
                Serial.println(" trying again later");
                mqttConnected = false;
            }
        }
        catch (const std::exception &e)
        {
            Serial.print("MQTT connection exception: ");
            Serial.println(e.what());
            mqttConnected = false;
        }
        catch (...)
        {
            Serial.println("MQTT connection failed with unknown exception");
            mqttConnected = false;
        }
    }
}

void setupMqtt()
{
    if (discoverMqttBroker())
    {
        checkMqttConnection(nullptr); // Initial connection attempt
    }
}
