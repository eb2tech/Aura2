#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include "forecast_mqtt.h"
#include "forecast_preferences.h"
#include "forecast_weather.h"
#include "forecast_widgets.h"

#define LCD_BACKLIGHT_PIN 21

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
bool mqttConnected = false;
bool discoveryPublished = false;

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    std::string message((char *)payload, length);
    String topicStr = String(topic);
    String deviceId = getDeviceIdentifier();
    JsonDocument doc;

    Log.infoln("Message arrived: %s", message.c_str());
    Log.infoln(" on topic: %s", topic);

    deserializeJson(doc, (char *)payload, length);

    // Handle backlight control commands
    if (topicStr == ("aura/" + deviceId + "/backlight/set"))
    {
        auto state = doc["state"].as<String>();
        auto brightnessValue = doc["brightness"].as<uint8_t>();

        if (state == "ON")
        {
            brightness = constrain(brightnessValue, 1, 255);

            analogWrite(LCD_BACKLIGHT_PIN, brightness);
            preferences.putUInt("brightness", brightness);

            Serial.println("Backlight turned ON via MQTT");
            publishBacklightState();
        }
        else if (state == "OFF")
        {
            brightness = 0;
            analogWrite(LCD_BACKLIGHT_PIN, 0);
            preferences.putUInt("brightness", brightness);
            Serial.println("Backlight turned OFF via MQTT");
            publishBacklightState();
        }
    }

    // mqttDispatcher.dispatch(topic, message);
}

bool discoverMqttBroker()
{
    if (!use_mqtt)
    {
        return false;
    }
    
    if (mqttServer != "")
    {
        Log.infoln("MQTT server already configured: %s", mqttServer.c_str());
        return true;
    }

    Log.infoln("Discovering MQTT broker via mDNS...");
    int n = MDNS.queryService("mqtt", "tcp");
    if (n == 0)
    {
        Log.infoln("No MQTT broker found via mDNS");
        return false;
    }
    else
    {
        mqttServer = MDNS.IP(0).toString();
        Log.infoln("Discovered MQTT broker: %s at %s", MDNS.hostname(0).c_str(), mqttServer.c_str());
        return true;
    }
}

void checkMqttConnection(lv_timer_t *timer)
{
    if (!use_mqtt)
    {
        return;
    }

    if (!mqttClient.connected())
    {
        Log.infoln("MQTT not connected, attempting to connect...");

        if (!discoverMqttBroker())
        {
            return;
        }

        mqttClient.setServer(mqttServer.c_str(), 1883);
        mqttClient.setCallback(mqttCallback);

        Log.infoln("MQTT client configured to connect to: %s", mqttServer.c_str());

        try
        {
            if (mqttClient.connect(getDeviceIdentifier().c_str(), mqttUser.c_str(), mqttPassword.c_str()))
            {
                Log.infoln("MQTT connected successfully");
                mqttConnected = true;

                // Publish Home Assistant discovery messages
                publishHomeAssistantDiscovery();
            }
            else
            {
                Log.errorln("MQTT connection failed, rc=%d", mqttClient.state());
                mqttConnected = false;
                discoveryPublished = false; // Reset discovery flag on disconnect
            }
        }
        catch (const std::exception &e)
        {
            Log.errorln("MQTT connection exception: %s", e.what());
            mqttConnected = false;
            discoveryPublished = false;
        }
        catch (...)
        {
            Log.errorln("MQTT connection failed with unknown exception");
            mqttConnected = false;
            discoveryPublished = false;
        }
    }
}

void setupMqtt()
{
    // Increase buffer size for Home Assistant discovery messages
    mqttClient.setBufferSize(1024);

    if (discoverMqttBroker())
    {
        checkMqttConnection(nullptr); // Initial connection attempt
    }
}

void loopMqtt()
{
    if (mqttConnected)
    {
        mqttClient.loop();
    }
}

void publishHomeAssistantDiscovery()
{
    if (!mqttConnected || discoveryPublished)
    {
        return;
    }

    String deviceId = getDeviceIdentifier();
    String baseTopic = "homeassistant";
    String state_topic = "aura/" + deviceId + "/temperature";

    // Device information for all entities
    JsonDocument deviceInfo;
    deviceInfo["identifiers"].add(deviceId);
    deviceInfo["name"] = "Aura2 Weather Display";
    deviceInfo["manufacturer"] = "Aura2";
    deviceInfo["model"] = ESP.getChipModel();

    // Temperature Sensor
    JsonDocument tempConfig;
    tempConfig["name"] = "Temperature";
    tempConfig["state_topic"] = state_topic;
    tempConfig["unit_of_measurement"] = "°C";
    tempConfig["device_class"] = "temperature";
    tempConfig["value_template"] = "{{ value_json.temperature }}";
    tempConfig["unique_id"] = deviceId + "_temperature";
    tempConfig["device"] = deviceInfo;

    // Feels Like Temperature Sensor
    JsonDocument feelsLikeConfig;
    feelsLikeConfig["name"] = "Feels Like Temperature";
    feelsLikeConfig["state_topic"] = state_topic;
    feelsLikeConfig["unit_of_measurement"] = "°C";
    feelsLikeConfig["device_class"] = "temperature";
    feelsLikeConfig["value_template"] = "{{ value_json.feels_like }}";
    feelsLikeConfig["unique_id"] = deviceId + "_feels_like";
    feelsLikeConfig["device"] = deviceInfo;

    String tempConfigTopic = baseTopic + "/sensor/" + deviceId + "_temp/config";
    String tempConfigPayload;
    serializeJson(tempConfig, tempConfigPayload);
    auto tempPublishResult = mqttClient.publish(tempConfigTopic.c_str(), tempConfigPayload.c_str(), true);

    String feelsLikeConfigTopic = baseTopic + "/sensor/" + deviceId + "_feels_like/config";
    String feelsLikeConfigPayload;
    serializeJson(feelsLikeConfig, feelsLikeConfigPayload);
    auto feelsLikePublishResult = mqttClient.publish(feelsLikeConfigTopic.c_str(), feelsLikeConfigPayload.c_str(), true);

    if (tempPublishResult && feelsLikePublishResult)
    {
        Log.infoln("Published Aura2 discovery");

        // Publish initial sensor states
        publishSensorState();
    }

    // Backlight Light Entity
    JsonDocument backlightConfig;
    backlightConfig["name"] = "Backlight";
    backlightConfig["command_topic"] = "aura/" + deviceId + "/backlight/set";
    backlightConfig["state_topic"] = "aura/" + deviceId + "/backlight/state";
    backlightConfig["schema"] = "json";
    backlightConfig["brightness"] = true;
    backlightConfig["unique_id"] = deviceId + "_backlight";
    backlightConfig["brightness_scale"] = 255;
    backlightConfig["device"] = deviceInfo;

    String backlightConfigTopic = baseTopic + "/light/" + deviceId + "_backlight/config";
    String backlightConfigPayload;
    serializeJson(backlightConfig, backlightConfigPayload);

    if (mqttClient.publish(backlightConfigTopic.c_str(), backlightConfigPayload.c_str(), true))
    {
        Log.infoln("Published backlight discovery");

        // Subscribe to backlight command topics
        String backlightSetTopic = "aura/" + deviceId + "/backlight/set";
        mqttClient.subscribe(backlightSetTopic.c_str());

        // Publish initial backlight state
        publishBacklightState();
    }

    discoveryPublished = true;
    Serial.println("Home Assistant discovery messages published");
}

void publishSensorState()
{
    if (!mqttConnected)
    {
        return;
    }

    String deviceId = getDeviceIdentifier();
    String topic = "aura/" + deviceId + "/temperature";

    JsonDocument statePayload;
    statePayload["temperature"] = temperature_now;
    statePayload["feels_like"] = feels_like_temperature;

    String payload;
    serializeJson(statePayload, payload);

    mqttClient.publish(topic.c_str(), payload.c_str());
}

void publishBacklightState()
{
    if (!mqttConnected)
    {
        return;
    }

    String deviceId = getDeviceIdentifier();
    auto backlightState = getBacklightState();

    // Publish state (ON/OFF)
    String stateTopic = "aura/" + deviceId + "/backlight/state";
    JsonDocument statePayload;
    statePayload["state"] = backlightState.isOn ? "ON" : "OFF";
    statePayload["brightness"] = backlightState.brightness;

    String payload;
    serializeJson(statePayload, payload);

    mqttClient.publish(stateTopic.c_str(), payload.c_str());
}
