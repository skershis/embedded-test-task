#pragma once

#include "config.hpp"
#include "gpio/gpio_imanager.hpp"
#include "mqtt/mqtt_iclient.hpp"
#include "safe_queue.hpp"
#include "temperature_sensor.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>

class Application
{
public:
    Application(const AppConfig &config,
                std::unique_ptr<mqtt::IClient> mqtt_client,
                std::unique_ptr<gpio::IManager> gpio_manager,
                std::unique_ptr<TemperatureSensor> temperature_sensor);
    ~Application();

    void run();
    void restart();

private:
    enum class State {
        WaitingToConnect,
        Connected,
        Disconnected,
        Reconnecting,
        Restarting,
        Exiting
    };

    void connectToMqtt();
    void setupGpioPins();
    void removeGpioPins();
    void setupGpioHandlers();
    void removeGpioHandlers();
    void setupMqttHandlers();
    void processIncomingMessage(const std::string &topic, const std::string &payload);
    void processButton();
    void processTemperatureSensor();

    void printMessage(const std::string &msg) const;
    void printError(const std::string &msg) const;

private:
    AppConfig config_;
    std::unique_ptr<mqtt::IClient> mqtt_client_;
    std::unique_ptr<gpio::IManager> gpio_manager_;
    std::unique_ptr<TemperatureSensor> temperature_sensor_;
    SafeQueue<std::pair<std::string, std::string>> incoming_messages_;

    State state_;
    int reconnect_attempts_;
    std::chrono::steady_clock::time_point last_reconnect_time_;
    bool led_state_;

    mutable std::mutex state_mutex_;
    mutable std::mutex log_mutex_;
};
