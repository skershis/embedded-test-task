#include "application.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

Application::Application(const AppConfig &config,
                         std::unique_ptr<mqtt::IClient> mqtt_client,
                         std::unique_ptr<gpio::IManager> gpio_manager,
                         std::unique_ptr<TemperatureSensor> temperature_sensor)
    : config_(config)
    , mqtt_client_(std::move(mqtt_client))
    , gpio_manager_(std::move(gpio_manager))
    , temperature_sensor_(std::move(temperature_sensor))
    , state_(State::WaitingToConnect)
    , reconnect_attempts_(0)
    , led_state_(false)
    , last_reconnect_time_(std::chrono::steady_clock::now())
{
    setupGpioPins();
    setupGpioHandlers();
}

Application::~Application() = default;

void Application::setupGpioPins()
{
    gpio_manager_->registerPin({config_.pins.red_pin, gpio::PinType::Analog, gpio::PinMode::Output});
    gpio_manager_->registerPin(
        {config_.pins.green_pin, gpio::PinType::Analog, gpio::PinMode::Output});
    gpio_manager_->registerPin(
        {config_.pins.blue_pin, gpio::PinType::Analog, gpio::PinMode::Output});
    gpio_manager_->registerPin(
        {config_.pins.temperature_pin, gpio::PinType::Analog, gpio::PinMode::Input});
    gpio_manager_->registerPin(
        {config_.pins.button_pin, gpio::PinType::Digital, gpio::PinMode::Input});
    gpio_manager_->registerPin(
        {config_.pins.led_pin, gpio::PinType::Digital, gpio::PinMode::Output});
}

void Application::removeGpioPins()
{
    gpio_manager_->unregisterPin(config_.pins.red_pin);
    gpio_manager_->unregisterPin(config_.pins.green_pin);
    gpio_manager_->unregisterPin(config_.pins.blue_pin);
    gpio_manager_->unregisterPin(config_.pins.temperature_pin);
    gpio_manager_->unregisterPin(config_.pins.button_pin);
    gpio_manager_->unregisterPin(config_.pins.led_pin);
}

void Application::setupGpioHandlers()
{
    gpio_manager_->setWriteDigitalCallback([this](int pin, gpio::DigitalValue value) {
        printMessage("[APP] Digital pin " + std::to_string(pin) + " changed to "
                     + (value == gpio::DigitalValue::High ? "HIGH" : "LOW"));

        nlohmann::json message;
        message["pin"] = pin;
        message["value"] = value;
        std::string payload = message.dump();

        printMessage("[APP] Publishing MQTT message to topic 'embedded/pins/state': " + payload);
        mqtt_client_->publish("embedded/pins/state", payload);
    });

    gpio_manager_->setWriteAnalogCallback([this](int pin, uint8_t value) {
        printMessage("[APP] Analog pin " + std::to_string(pin) + " set to " + std::to_string(value));

        nlohmann::json message;
        message["pin"] = pin;
        message["value"] = value;
        std::string payload = message.dump();

        printMessage("[APP] Publishing MQTT message to topic 'embedded/pins/state': " + payload);
        mqtt_client_->publish("embedded/pins/state", payload);
    });
}

void Application::removeGpioHandlers()
{
    gpio_manager_->setWriteDigitalCallback(nullptr);
    gpio_manager_->setWriteAnalogCallback(nullptr);
}

void Application::setupMqttHandlers()
{
    mqtt_client_->setMessageCallback([this](const std::string &topic, const std::string &payload) {
        incoming_messages_.emplace(topic, payload);
    });

    mqtt_client_->setConnectCallback([this]() {
        printMessage("[APP] MQTT Client Connected");
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_ = State::Connected;
            reconnect_attempts_ = 0;
        }
    });

    mqtt_client_->setDisconnectCallback([this](int reason) {
        printMessage("[APP] MQTT Client Disconnected, reason = " + std::to_string(reason));
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (state_ != State::Restarting) {
                state_ = State::Disconnected;
                last_reconnect_time_ = std::chrono::steady_clock::now();
            }
        }
    });
}

void Application::connectToMqtt()
{
    try {
        mqtt_client_->connect();
        mqtt_client_->subscribe("embedded/control");
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_ = State::WaitingToConnect;
        }
    } catch (const std::exception &e) {
        printError("[APP] MQTT initial client connect failed: " + std::string(e.what()));
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_ = State::Disconnected;
            last_reconnect_time_ = std::chrono::steady_clock::now();
        }
    }
}

void Application::processIncomingMessage(const std::string &topic, const std::string &payload)
{
    printMessage("[APP] MQTT message received: [" + topic + "] " + payload);

    nlohmann::json data;
    try {
        data = nlohmann::json::parse(payload);
    } catch (const std::exception &e) {
        mqtt_client_->publish("embedded/errors", "Invalid JSON format: " + std::string(e.what()));
        return;
    }

    if (!data.contains("command") || !data["command"].is_string()) {
        mqtt_client_->publish("embedded/errors", "Missing or invalid 'command' field");
        return;
    }

    const std::string command = data["command"];

    if (topic == "embedded/control" && command == "restart") {
        printMessage("[APP] Received restart command");
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_ = State::Restarting;
        }
        return;
    }

    if (topic == "embedded/control" && command == "set_rgb") {
        static constexpr int color_min = 0;
        static constexpr int color_max = 255;

        // Проверка наличия и типа
        if (!data.contains("red") || !data.contains("green") || !data.contains("blue")
            || !data["red"].is_number_integer() || !data["green"].is_number_integer()
            || !data["blue"].is_number_integer()) {
            mqtt_client_->publish("embedded/errors",
                                  "Missing or invalid 'red', 'green', or 'blue' fields");
            return;
        }

        int red = data["red"];
        int green = data["green"];
        int blue = data["blue"];

        if (red < color_min || red > color_max || green < color_min || green > color_max
            || blue < color_min || blue > color_max) {
            mqtt_client_->publish("embedded/errors", "RGB values must be in range [0, 255]");
            return;
        }

        printMessage("[APP] Received RGB command: R=" + std::to_string(red)
                     + " G=" + std::to_string(green) + " B=" + std::to_string(blue));

        try {
            gpio_manager_->writeAnalogPin(config_.pins.red_pin, static_cast<uint8_t>(red));
            gpio_manager_->writeAnalogPin(config_.pins.green_pin, static_cast<uint8_t>(green));
            gpio_manager_->writeAnalogPin(config_.pins.blue_pin, static_cast<uint8_t>(blue));
        } catch (const std::exception &e) {
            mqtt_client_->publish("embedded/errors", "GPIO error: " + std::string(e.what()));
        }

        return;
    }

    // Неизвестная команда или топик
    mqtt_client_->publish("embedded/errors", "Unsupported command or topic: " + command);
}

void Application::processButton()
{
    auto button_state = gpio_manager_->readDigitalPin(config_.pins.button_pin);
    if (button_state == gpio::DigitalValue::High) {
        led_state_ = !led_state_;
        gpio_manager_->writeDigitalPin(config_.pins.led_pin,
                                       led_state_ ? gpio::DigitalValue::High
                                                  : gpio::DigitalValue::Low);
    }
}

void Application::restart()
{
    static constexpr int restart_timeout_s = 3;

    mqtt_client_->disconnect();
    removeGpioPins();
    removeGpioHandlers();

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_ = State::Disconnected;
    }

    printMessage("[APP] Restarting...");
    std::this_thread::sleep_for(std::chrono::seconds(restart_timeout_s));

    // конфигурируем заново gpio
    setupGpioPins();
    setupGpioHandlers();
}

void Application::processTemperatureSensor()
{
    static constexpr auto temperature_publish_timeout_s = 5;

    static constexpr struct AnalogRange
    {
        int Min = 0;
        int Max = 255;
    } analog_range;

    static constexpr struct TemperatureRange
    {
        int Min = 200;
        int Max = 300;
    } temperature_range;

    static auto last_temp_time = std::chrono::steady_clock::now();

    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_temp_time).count()
        >= temperature_publish_timeout_s) {
        int temperature = temperature_sensor_->getTemperatureTenthCelsius();

        uint8_t analog = (temperature - temperature_range.Min) * analog_range.Max
                         / (temperature_range.Max - temperature_range.Min);

        gpio_manager_->injectAnalogValue(config_.pins.temperature_pin, analog);

        uint8_t raw = gpio_manager_->readAnalogPin(config_.pins.temperature_pin);
        temperature = raw * (temperature_range.Max - temperature_range.Min) / analog_range.Max
                      + temperature_range.Min;

        std::string payload = "{\"temperature\":" + std::to_string(temperature) + "}";
        mqtt_client_->publish("embedded/sensors/temperature", payload);

        printMessage("[APP] Published temperature: " + payload);
        last_temp_time = now;
    }
}

void Application::run()
{
    setupMqttHandlers();
    connectToMqtt();

    constexpr int reconnect_interval_ms = 2000;

    bool is_running = true;

    while (is_running) {
        State current_state;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            current_state = state_;
        }

        auto now = std::chrono::steady_clock::now();

        switch (current_state) {
        case State::WaitingToConnect:
            break;

        case State::Connected: {
            processButton();
            processTemperatureSensor();

            if (auto msg = incoming_messages_.pop(0)) {
                const auto &[topic, payload] = *msg;
                processIncomingMessage(topic, payload);
            }
            break;
        }

        case State::Disconnected: {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_reconnect_time_)
                    .count()
                >= reconnect_interval_ms) {
                {
                    std::lock_guard<std::mutex> lock(state_mutex_);                 
                    if (reconnect_attempts_ < config_.max_reconnect_attempts) {
                        printMessage("[APP] Attempting reconnect MQTT connection, attempt "
                                     + std::to_string(reconnect_attempts_ + 1));
                        state_ = State::Reconnecting;
                    } else {
                        printError(
                            "[APP] Max reconnection attempts reached, getting application to exit");
                        last_reconnect_time_ = now;
                        state_ = State::Exiting;
                    }
                }
            }
            break;
        }

        case State::Reconnecting: {
            try {
                if (mqtt_client_->isConnected()) {
                    mqtt_client_->disconnect();
                }
                mqtt_client_->connect();
                mqtt_client_->subscribe("embedded/control");
                {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    state_ = state_ != State::Connected ? State::WaitingToConnect
                                                        : State::Connected;
                    reconnect_attempts_ = 0;
                }
            } catch (const std::exception &e) {
                printError("[APP] Reconnect failed: " + std::string(e.what()));
                {
                    std::lock_guard<std::mutex> lock(state_mutex_);
                    state_ = State::Disconnected;
                    last_reconnect_time_ = now;
                    reconnect_attempts_++;
                }
            }
            break;
        }

        case State::Restarting: {
            restart();
            break;
        }

        case State::Exiting: {
            printMessage("[APP] Exiting application");
            is_running = false;
            break;
        }
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void Application::printMessage(const std::string &msg) const
{
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::cout << msg << std::endl;
}

void Application::printError(const std::string &msg) const
{
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::cerr << msg << std::endl;
}
