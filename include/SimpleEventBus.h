#pragma once
#include <Arduino.h>
#include <functional>
#include <vector>
#include <map>

class SimpleEventBus
{
private:
    std::map<String, std::vector<std::function<void(String)>>> subscribers;

public:
    void subscribe(String event, std::function<void(String)> callback)
    {
        subscribers[event].push_back(callback);
    }

    void publish(String event, String data = "")
    {
        if (subscribers.find(event) != subscribers.end())
        {
            for (auto &callback : subscribers[event])
            {
                callback(data);
            }
        }
    }
};

// Usage:
/*
SimpleEventBus eventBus;

void setup() {
    eventBus.subscribe("weather_update", [](String data) {
        Serial.println("Weather: " + data);
    });

    eventBus.subscribe("mqtt_message", [](String data) {
        Serial.println("MQTT: " + data);
    });
}

void publishWeather() {
    eventBus.publish("weather_update", "25.3°C");
}
*/