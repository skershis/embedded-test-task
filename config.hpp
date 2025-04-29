#pragma once

struct PinConfig
{
    int red_pin;
    int green_pin;
    int blue_pin;
    int temperature_pin;
    int button_pin;
    int led_pin;
};

struct AppConfig
{
    int max_reconnect_attempts;
    PinConfig pins;
};
