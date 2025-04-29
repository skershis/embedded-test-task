#pragma once

#include "mqtt_iclient.hpp"
#include "safe_queue.hpp"
#include <functional>
#include <mosquitto.h>
#include <string>
#include <thread>

namespace mqtt {

class Client : public IClient
{
public:
    using MessageCallback = std::function<void(const std::string &, const std::string &)>;
    using ConnectCallback = std::function<void()>;
    using DisconnectCallback = std::function<void(int reason_code)>;

    Client(const std::string &host,
           int port,
           const std::string &client_id,
           const std::string &client_username,
           const std::string &client_password);

    ~Client();

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;
    Client(Client &&) noexcept = delete;
    Client &operator=(Client &&) noexcept = delete;

    void connect() override final;
    void disconnect() override final;
    bool isConnected() override final;
    void subscribe(const std::string &topic) override final;
    void publish(const std::string &topic, const std::string &payload) override final;
    void setMessageCallback(MessageCallback callback) override final;
    void setConnectCallback(ConnectCallback callback) override final;
    void setDisconnectCallback(DisconnectCallback callback) override final;

private:
    static void onConnectWrapper(struct mosquitto *, void *, int rc);
    static void onDisconnectWrapper(struct mosquitto *, void *, int rc);
    static void onMessageWrapper(struct mosquitto *, void *, const struct mosquitto_message *);

    void onConnect(int rc);
    void onDisconnect(int rc);
    void onMessage(const struct mosquitto_message *msg);
    void loop(int timeout_ms);

    void printMessage(const std::string &msg);
    void printError(const std::string &msg);

private:
    std::string id_;
    std::string username_;
    std::string password_;
    std::string host_;
    int port_;

    bool running_{false};
    std::thread loop_thread_;
    SafeQueue<std::pair<std::string, std::string>> publish_queue_;

    MessageCallback message_callback_ = nullptr;
    ConnectCallback connect_callback_ = nullptr;
    DisconnectCallback disconnect_callback_ = nullptr;

    mutable std::mutex mutex_;

    struct mosquitto *mosq_ = nullptr;
};

} // namespace mqtt
