#pragma once

#include "gpio_imanager.hpp"
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace gpio {

class Manager : public IManager
{
public:
    using WriteDigitalCallback = std::function<void(int pin_number, DigitalValue value)>;
    using WriteAnalogCallback = std::function<void(int pin_number, uint8_t value)>;

    Manager();
    ~Manager();

    Manager(const Manager &) = delete;
    Manager &operator=(const Manager &) = delete;
    Manager(Manager &&other) noexcept;
    Manager &operator=(Manager &&other) noexcept;

    void registerPin(const PinConfig &config) override final;
    void unregisterPin(int pin_number) override final;

    void writeDigitalPin(int pin_number, DigitalValue value) override final;
    void writeAnalogPin(int pin_number, uint8_t value) override final;

    DigitalValue readDigitalPin(int pin_number) override final;
    uint8_t readAnalogPin(int pin_number) override final;

    void setWriteDigitalCallback(WriteDigitalCallback cb) override final;
    void setWriteAnalogCallback(WriteAnalogCallback cb) override final;

    void injectAnalogValue(int pin_number, uint8_t value) override final;

private:
    struct PinState
    {
        PinType type;
        PinMode mode;
        uint8_t value;
    };

    mutable std::mutex mutex_;
    std::unordered_map<int, PinState> pins_;
    WriteDigitalCallback writeDigitalCallback_;
    WriteAnalogCallback writeAnalogCallback_;

    void triggerWriteDigitalCallback(int pin_number, DigitalValue value);
    void triggerWriteAnalogCallback(int pin_number, uint8_t value);
};

} // namespace gpio
