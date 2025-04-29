#pragma once

#include "gpio_types.hpp"
#include <stdint.h>

#include <functional>

namespace gpio {

class IManager
{
public:
    virtual ~IManager() = default;

    virtual void registerPin(const PinConfig& config) = 0;
    virtual void unregisterPin(int pin_number) = 0;

    virtual void writeDigitalPin(int pin_number, gpio::DigitalValue value) = 0;
    virtual void writeAnalogPin(int pin_number, uint8_t value) = 0;

    virtual gpio::DigitalValue readDigitalPin(int pin_number) = 0;
    virtual uint8_t readAnalogPin(int pin_number) = 0;

    virtual void injectAnalogValue(int pin_number, uint8_t value) = 0;

    virtual void setWriteDigitalCallback(std::function<void(int, gpio::DigitalValue)> callback) = 0;
    virtual void setWriteAnalogCallback(std::function<void(int, uint8_t)> callback) = 0;
};
} // namespace gpio
