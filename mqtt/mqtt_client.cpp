#include "mqtt_client.hpp"
#include <iostream>

namespace mqtt {

Client::Client(const std::string &host,
               int port,
               const std::string &client_id,
               const std::string &client_username,
               const std::string &client_password)
    : id_(client_id)
    , username_(client_username)
    , password_(client_password)
    , host_(host)
    , port_(port)
{
    mosquitto_lib_init();

    mosq_ = mosquitto_new(id_.c_str(), true, this);
    if (!mosq_) {
        throw std::runtime_error("Failed to create Mosquitto instance");
    }

    if (!username_.empty() && !password_.empty()) {
        int rc = mosquitto_username_pw_set(mosq_, username_.c_str(), password_.c_str());
        if (rc != MOSQ_ERR_SUCCESS) {
            throw std::runtime_error("Failed to set username/password: "
                                     + std::string(mosquitto_strerror(rc)));
        }
    }

    mosquitto_connect_callback_set(mosq_, &Client::onConnectWrapper);
    mosquitto_disconnect_callback_set(mosq_, &Client::onDisconnectWrapper);
    mosquitto_message_callback_set(mosq_, &Client::onMessageWrapper);
}

Client::~Client()
{
    disconnect();
    if (mosq_) {
        mosquitto_destroy(mosq_);
    }
    mosquitto_lib_cleanup();
}

void Client::connect()
{
    static constexpr int keepalive = 60;
    static constexpr int mqtt_timeout_ms = 100;

    int rc = mosquitto_connect(mosq_, host_.c_str(), port_, keepalive);
    if (rc != MOSQ_ERR_SUCCESS) {
        throw std::runtime_error("Failed to connect to MQTT broker: "
                                 + std::string(mosquitto_strerror(rc)));
    }

    running_ = true;
    loop_thread_ = std::thread([this] { loop(mqtt_timeout_ms); });
}

void Client::disconnect()
{
    mosquitto_disconnect(mosq_);

    running_ = false;
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
}

bool Client::isConnected()
{
    return loop_thread_.joinable();
}

void Client::subscribe(const std::string &topic)
{
    std::lock_guard<std::mutex> lock(mutex_);

    int rc = mosquitto_subscribe(mosq_, nullptr, topic.c_str(), 0);
    if (rc != MOSQ_ERR_SUCCESS) {
        throw std::runtime_error("Failed to subscribe: " + std::string(mosquitto_strerror(rc)));
    }
}

void Client::publish(const std::string &topic, const std::string &payload)
{
    publish_queue_.emplace(topic, payload);
}

void Client::setMessageCallback(MessageCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    message_callback_ = std::move(callback);
}

void Client::setConnectCallback(ConnectCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    connect_callback_ = std::move(callback);
}

void Client::setDisconnectCallback(DisconnectCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    disconnect_callback_ = std::move(callback);
}

void Client::loop(int timeout_ms)
{
    while (running_) {
        int rc = mosquitto_loop(mosq_, timeout_ms, 1);
        if (rc != MOSQ_ERR_SUCCESS && running_) {
            printError("[MQTT_CLIENT] loop error: " + std::string(mosquitto_strerror(rc)));
            break;
        }

        if (auto item = publish_queue_.pop(0)) {
            const auto &[topic, payload] = *item;
            int rc_pub = mosquitto_publish(mosq_,
                                           nullptr,
                                           topic.c_str(),
                                           payload.size(),
                                           payload.c_str(),
                                           0,
                                           false);
            if (rc_pub != MOSQ_ERR_SUCCESS) {
                printError("[MQTT_CLIENT] Publish failed: "
                           + std::string(mosquitto_strerror(rc_pub)));
            }
        }
    }
}

void Client::onConnectWrapper(struct mosquitto *mosq, void *obj, int rc)
{
    if (auto *self = static_cast<Client *>(obj)) {
        self->onConnect(rc);
    }
}

void Client::onDisconnectWrapper(struct mosquitto *mosq, void *obj, int rc)
{
    if (auto *self = static_cast<Client *>(obj)) {
        self->onDisconnect(rc);
    }
}

void Client::onMessageWrapper(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
    if (auto *self = static_cast<Client *>(obj)) {
        self->onMessage(msg);
    }
}

void Client::onConnect(int rc)
{
    if (rc == 0) {
        printMessage("[MQTT_CLIENT] Connected successfully");
        if (connect_callback_) {
            connect_callback_();
        }
    } else {
        printError("[MQTT_CLIENT] Connection failed: " + std::to_string(rc));
    }
}

void Client::onDisconnect(int rc)
{
    printMessage("[MQTT_CLIENT] Disconnected: " + std::to_string(rc));
    running_ = false;

    if (disconnect_callback_) {
        disconnect_callback_(rc);
    }
}

void Client::onMessage(const struct mosquitto_message *msg)
{
    if (message_callback_ && msg && msg->payload) {
        std::string topic = msg->topic ? msg->topic : "";
        std::string payload(static_cast<char *>(msg->payload), msg->payloadlen);
        message_callback_(topic, payload);
    }
}

void Client::printMessage(const std::string &msg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << msg << std::endl;
}

void Client::printError(const std::string &msg)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << msg << std::endl;
}

} // namespace mqtt
