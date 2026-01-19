#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "forecast_nats.h"
#include "forecast_preferences.h"

// Note: ArduinoLog is not used in this file to avoid a circular dependency with logging.

WiFiClient natsWifiClient;
PubSubClient natsClient(natsWifiClient);
bool natsConnected = false;

void checkNatsConnection(lv_timer_t *timer)
{
    if (!use_nats)
    {
        disconnectNats();
        return;
    }

    setupNats();

    connectNats();
}

void connectNats()
{
    if (!natsConnected)
    {
        try
        {
            if (natsClient.connect(getDeviceIdentifier().c_str(), natsUser.c_str(), natsPassword.c_str()))
            {
                Serial.println("NATS connected successfully");
                natsConnected = true;
            }
            else
            {
                Serial.printf("NATS connection failed, rc=%d\n", natsClient.state());
                natsConnected = false;
            }
        }
        catch (const std::exception &e)
        {
            Serial.printf("NATS connection exception: %s\n", e.what());
            natsConnected = false;
        }
        catch (...)
        {
            Serial.println("NATS connection failed with unknown exception");
            natsConnected = false;
        }
    }
}

void disconnectNats()
{
    if (natsConnected)
    {
        natsClient.disconnect();
        natsConnected = false;
        Serial.println("NATS disconnected");
    }
}

void setupNats()
{
    if (!use_nats)
    {
        return;
    }
    
    natsClient.setBufferSize(1024);
    natsClient.setServer(natsServer.c_str(), 1883);
    Serial.println("NATS client configured to connect to: " + natsServer + " (" + natsUser + ", " + natsPassword + ")");

    connectNats();
}

void loopNats()
{
    if (natsConnected)
    {
        natsClient.loop();
    }
}

void publishLogMessage(const char* message)
{
    if (!natsConnected)
    {
        return;
    }

    JsonDocument logMessage;
    logMessage["timestamp"] = millis();
    logMessage["message"] = message;
    logMessage["message_length"] = strlen(message);

    String logPayload;
    serializeJson(logMessage, logPayload);

    String subject = "aura2/logs/" + getDeviceIdentifier();
    natsClient.publish(subject.c_str(), logPayload.c_str());
}