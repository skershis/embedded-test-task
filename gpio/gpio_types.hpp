#pragma once

namespace gpio {

// Тип пина: цифровой или аналоговый
enum class PinType {
    Digital,
    Analog
};

// Режим работы пина: вход или выход
enum class PinMode {
    Input,
    Output
};

// Значение для цифрового пина
enum class DigitalValue {
    Low,
    High
};

struct PinConfig
{
    int number;
    PinType type;
    PinMode mode;
};

} // namespace gpio
