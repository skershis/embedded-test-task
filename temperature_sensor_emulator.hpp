#pragma once

#include "temperature_sensor.hpp"
#include <random>

template<int MinTempTenthC, int MaxTempTenthC>
class TemperatureSensorEmulator : public TemperatureSensor
{
public:
    TemperatureSensorEmulator()
        : dist_(MinTempTenthC, MaxTempTenthC)
    {}

    int getTemperatureTenthCelsius() override { return dist_(gen_); }

private:
    std::mt19937 gen_{std::random_device{}()};
    std::uniform_int_distribution<int> dist_;
};
