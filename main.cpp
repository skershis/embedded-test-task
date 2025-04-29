#include "application.hpp"
#include "config.hpp"
#include "gpio/gpio_manager.hpp"
#include "mqtt/mqtt_client.hpp"
#include "temperature_sensor_emulator.hpp"

#include <cstdlib> // std::getenv
#include <iostream>
#include <memory>
#include <string>

std::string getEnvVar(const std::string &key, const std::string &default_value = "")
{
    if (const char *val = std::getenv(key.c_str())) {
        return std::string(val);
    }
    return default_value;
}

int getEnvVarInt(const std::string &key, int default_value)
{
    if (const char *val = std::getenv(key.c_str())) {
        return std::stoi(val);
    }
    return default_value;
}

int main()
{
    try {
        // Заполняем AppConfig
        AppConfig app_config{.max_reconnect_attempts = getEnvVarInt("MAX_RECONNECT_ATTEMPTS", 5),
                             .pins = PinConfig{.red_pin = getEnvVarInt("RED_PIN", 3),
                                               .green_pin = getEnvVarInt("GREEN_PIN", 5),
                                               .blue_pin = getEnvVarInt("BLUE_PIN", 6),
                                               .temperature_pin = getEnvVarInt("TEMPERATURE_PIN", 0),
                                               .button_pin = getEnvVarInt("BUTTON_PIN", 2),
                                               .led_pin = getEnvVarInt("LED_PIN", 13)}};

        std::unique_ptr<mqtt::IClient> mqtt_client
            = std::make_unique<mqtt::Client>(getEnvVar("MQTT_HOST", "localhost"),
                                             getEnvVarInt("MQTT_PORT", 1883),
                                             getEnvVar("MQTT_CLIENT_ID", "embedded_device"),
                                             getEnvVar("MQTT_USERNAME", ""),
                                             getEnvVar("MQTT_PASSWORD", ""));

        std::unique_ptr<gpio::IManager> gpio_manager = std::make_unique<gpio::Manager>();

        std::unique_ptr<TemperatureSensor> temp_sensor
            = std::make_unique<TemperatureSensorEmulator<200, 300>>();

        Application app(app_config,
                        std::move(mqtt_client),
                        std::move(gpio_manager),
                        std::move(temp_sensor));

        app.run();
    } catch (const std::exception &ex) {
        std::cerr << "[MAIN] Unhandled exception: " << ex.what() << std::endl;
        return 1;
    }
}
