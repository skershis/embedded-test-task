#pragma once

class TemperatureSensor {
public:
    virtual ~TemperatureSensor() = default;
    virtual int getTemperatureTenthCelsius() = 0;
};
