#include "gpio_manager.hpp"
#include <stdexcept>

namespace gpio {

Manager::Manager() = default;
Manager::~Manager() = default;

Manager::Manager(Manager &&other) noexcept
{
    std::lock_guard<std::mutex> lock(other.mutex_);
    pins_ = std::move(other.pins_);
    writeDigitalCallback_ = std::move(other.writeDigitalCallback_);
    writeAnalogCallback_ = std::move(other.writeAnalogCallback_);
}

Manager &Manager::operator=(Manager &&other) noexcept
{
    if (this != &other) {
        {
            std::lock_guard<std::mutex> lock_this(mutex_);
            std::lock_guard<std::mutex> lock_other(other.mutex_);
            pins_ = std::move(other.pins_);
            writeDigitalCallback_ = std::move(other.writeDigitalCallback_);
            writeAnalogCallback_ = std::move(other.writeAnalogCallback_);
        }
    }
    return *this;
}

void Manager::registerPin(const PinConfig &config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto [it, inserted] = pins_.emplace(config.number, PinState{config.type, config.mode, 0});
    if (!inserted) {
        throw std::runtime_error("Pin already registered: " + std::to_string(config.number));
    }
}

void Manager::unregisterPin(int pin_number)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pins_.find(pin_number);
    if (it == pins_.end()) {
        throw std::runtime_error("Pin not registered: " + std::to_string(pin_number));
    }
    pins_.erase(it);
}

void Manager::writeDigitalPin(int pin_number, DigitalValue value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pins_.find(pin_number);
    if (it == pins_.end()) {
        throw std::runtime_error("Pin not registered: " + std::to_string(pin_number));
    }
    if (it->second.mode != PinMode::Output || it->second.type != PinType::Digital) {
        throw std::runtime_error("Attempt to write to non-digital output pin: "
                                 + std::to_string(pin_number));
    }

    it->second.value = static_cast<uint8_t>((value == DigitalValue::High) ? 1 : 0);
    triggerWriteDigitalCallback(pin_number, value);
}

void Manager::writeAnalogPin(int pin_number, uint8_t value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pins_.find(pin_number);
    if (it == pins_.end()) {
        throw std::runtime_error("Pin not registered: " + std::to_string(pin_number));
    }
    if (it->second.mode != PinMode::Output || it->second.type != PinType::Analog) {
        throw std::runtime_error("Attempt to write to non-analog output pin: "
                                 + std::to_string(pin_number));
    }

    it->second.value = value;
    triggerWriteAnalogCallback(pin_number, value);
}

DigitalValue Manager::readDigitalPin(int pin_number)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pins_.find(pin_number);
    if (it == pins_.end()) {
        throw std::runtime_error("Pin not registered: " + std::to_string(pin_number));
    }
    if (it->second.type != PinType::Digital) {
        throw std::runtime_error("Attempt to read non-digital pin: " + std::to_string(pin_number));
    }

    return (it->second.value == 0) ? DigitalValue::Low : DigitalValue::High;
}

uint8_t Manager::readAnalogPin(int pin_number)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pins_.find(pin_number);
    if (it == pins_.end()) {
        throw std::runtime_error("Pin not registered: " + std::to_string(pin_number));
    }
    if (it->second.type != PinType::Analog) {
        throw std::runtime_error("Attempt to read non-analog pin: " + std::to_string(pin_number));
    }

    return it->second.value;
}

void Manager::setWriteDigitalCallback(WriteDigitalCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    writeDigitalCallback_ = std::move(cb);
}

void Manager::setWriteAnalogCallback(WriteAnalogCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    writeAnalogCallback_ = std::move(cb);
}

void Manager::triggerWriteDigitalCallback(int pin_number, DigitalValue value)
{
    if (writeDigitalCallback_) {
        writeDigitalCallback_(pin_number, value);
    }
}

void Manager::triggerWriteAnalogCallback(int pin_number, uint8_t value)
{
    if (writeAnalogCallback_) {
        writeAnalogCallback_(pin_number, value);
    }
}

void Manager::injectAnalogValue(int pin_number, uint8_t value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pins_.find(pin_number);
    if (it == pins_.end()) {
        throw std::runtime_error("Pin not registered: " + std::to_string(pin_number));
    }

    auto &state = it->second;
    if (state.type != PinType::Analog || state.mode != PinMode::Input) {
        throw std::runtime_error("Pin is not analog input: " + std::to_string(pin_number));
    }

    state.value = value;
}

} // namespace gpio
