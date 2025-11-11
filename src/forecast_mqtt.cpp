#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "forecast_mqtt.h"
#include "forecast_preferences.h"
#include "forecast_weather.h"

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

    Serial.print("Message arrived: ");
    Serial.print(message.c_str());
    Serial.print(" on topic: ");
    Serial.println(topic);

    deserializeJson(doc, (char *)payload, length);

    // Handle backlight control commands
    if (topicStr == ("aura/" + deviceId + "/backlight/set"))
    {
        auto state = doc["state"].as<String>();

        if (state == "ON")
        {
            if (brightness == 0)
            {
                brightness = 128; // Default brightness when turning on
                preferences.putUInt("brightness", brightness);
            }
                analogWrite(LCD_BACKLIGHT_PIN, brightness);

                Serial.println("Backlight turned ON via MQTT");
                publishSensorStates();
        }
        else if (state == "OFF")
        {
            analogWrite(LCD_BACKLIGHT_PIN, 0);
            Serial.println("Backlight turned OFF via MQTT");
            publishSensorStates();
        }
    }
    else if (topicStr == ("aura/" + deviceId + "/backlight/brightness/set"))
    {
        int newBrightness = doc["brightness"].as<int>();
        brightness = constrain(newBrightness, 0, 255);
        preferences.putUInt("brightness", brightness);
        analogWrite(LCD_BACKLIGHT_PIN, brightness);
        Serial.println("Backlight brightness set to " + String(brightness) + " via MQTT");
        publishSensorStates();
    }

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
            if (mqttClient.connect(getDeviceIdentifier().c_str(), mqttUser.c_str(), mqttPassword.c_str()))
            {
                Serial.println("MQTT connected successfully");
                mqttConnected = true;
                
                // Publish Home Assistant discovery messages
                publishHomeAssistantDiscovery();
            }
            else
            {
                Serial.print("MQTT connection failed, rc=");
                Serial.print(mqttClient.state());
                Serial.println(" trying again later");
                mqttConnected = false;
                discoveryPublished = false; // Reset discovery flag on disconnect
            }
        }
        catch (const std::exception &e)
        {
            Serial.print("MQTT connection exception: ");
            Serial.println(e.what());
            mqttConnected = false;
            discoveryPublished = false;
        }
        catch (...)
        {
            Serial.println("MQTT connection failed with unknown exception");
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
    
    // Device information for all entities
    JsonDocument deviceInfo;
    deviceInfo["ids"] = deviceId;
    deviceInfo["name"] = "Aura2 Weather Display";
    deviceInfo["mf"] = "Aura";
    deviceInfo["mdl"] = ESP.getChipModel();
    deviceInfo["sw"] = "1.0.0";

    // Origin
    JsonDocument originInfo;
    originInfo["name"] = "aura2mqtt";
    originInfo["sw"] = "1.0.0";
    originInfo["url"] = "https://github.com/eb2tech/Aura2";

    // Temperature Sensor
    JsonDocument tempConfig;
    tempConfig["p"] = "sensor";
    tempConfig["device_class"] = "temperature";
    tempConfig["unit_of_measurement"] = "°C";
    tempConfig["value_template"] = "{{ value_json.temperature }}";
    tempConfig["unique_id"] = deviceId + "_temperature";
    tempConfig["name"] = "Temperature";

    // Feels Like Temperature Sensor
    JsonDocument feelsLikeConfig;
    feelsLikeConfig["p"] = "sensor";
    feelsLikeConfig["device_class"] = "temperature";
    feelsLikeConfig["unit_of_measurement"] = "°C";
    feelsLikeConfig["value_template"] = "{{ value_json.feels_like }}";
    feelsLikeConfig["unique_id"] = deviceId + "_feels_like";
    feelsLikeConfig["name"] = "Feels Like Temperature";

    JsonDocument configurationInfo;
    configurationInfo["dev"] = deviceInfo;
    configurationInfo["o"] = originInfo;
    configurationInfo["cmps"].add(tempConfig);
    configurationInfo["cmps"].add(feelsLikeConfig);
    configurationInfo["state_topic"] = "aura2/" + deviceId + "/state";

    String configTopic = baseTopic + "/device/" + deviceId + "/config";
    String configPayload;
    serializeJson(configurationInfo, configPayload);
    
    if (mqttClient.publish(configTopic.c_str(), configPayload.c_str(), true))
    {
        Serial.println("Published Aura2 discovery");

        // Publish initial sensor states
        publishSensorStates();
    }

    // // Backlight Light Entity
    // JsonDocument backlightConfig;
    // backlightConfig["name"] = "Backlight";
    // backlightConfig["unique_id"] = deviceId + "_backlight";
    // backlightConfig["state_topic"] = "aura/" + deviceId + "/backlight/state";
    // backlightConfig["command_topic"] = "aura/" + deviceId + "/backlight/set";
    // backlightConfig["brightness_state_topic"] = "aura/" + deviceId + "/backlight/brightness";
    // backlightConfig["brightness_command_topic"] = "aura/" + deviceId + "/backlight/brightness/set";
    // backlightConfig["brightness_scale"] = 255;
    // backlightConfig["schema"] = "json";
    // backlightConfig["icon"] = "mdi:brightness-6";
    // backlightConfig["device"] = deviceInfo;

    // String backlightConfigTopic = baseTopic + "/light/" + deviceId + "_backlight/config";
    // String backlightConfigPayload;
    // serializeJson(backlightConfig, backlightConfigPayload);
    
    // if (mqttClient.publish(backlightConfigTopic.c_str(), backlightConfigPayload.c_str(), true))
    // {
    //     Serial.println("Published backlight discovery");
        
    //     // Subscribe to backlight command topics
    //     String backlightSetTopic = "aura/" + deviceId + "/backlight/set";
    //     String brightnessSetTopic = "aura/" + deviceId + "/backlight/brightness/set";
        
    //     mqttClient.subscribe(backlightSetTopic.c_str());
    //     mqttClient.subscribe(brightnessSetTopic.c_str());
        
    //     // Publish initial backlight state
    //     publishBacklightState();
    // }

    discoveryPublished = true;
    Serial.println("Home Assistant discovery messages published");
}

void publishTemperature(float temperature)
{
    if (!mqttConnected)
    {
        return;
    }

    String deviceId = getDeviceIdentifier();
    String topic = "aura/" + deviceId + "/temperature";
    String payload = String(temperature, 1);
    
    mqttClient.publish(topic.c_str(), payload.c_str());
}

void publishFeelsLikeTemperature(float feelsLike)
{
    if (!mqttConnected)
    {
        return;
    }

    String deviceId = getDeviceIdentifier();
    String topic = "aura/" + deviceId + "/feels_like";
    String payload = String(feelsLike, 1);
    
    mqttClient.publish(topic.c_str(), payload.c_str());
}

void publishSensorStates()
{
    if (!mqttConnected)
    {
        return;
    }

    String deviceId = getDeviceIdentifier();
    String topic = "aura/" + deviceId + "/state";

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
    
    // Publish state (ON/OFF)
    String stateTopic = "aura/" + deviceId + "/backlight/state";
    String statePayload = (brightness > 0) ? "ON" : "OFF";
    mqttClient.publish(stateTopic.c_str(), statePayload.c_str());
    
    // Publish brightness level
    String brightnessTopic = "aura/" + deviceId + "/backlight/brightness";
    String brightnessPayload = String(brightness);
    mqttClient.publish(brightnessTopic.c_str(), brightnessPayload.c_str());
}
