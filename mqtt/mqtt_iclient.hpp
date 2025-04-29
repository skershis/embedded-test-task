#pragma once

#include <string>
#include <functional>

namespace mqtt {
class IClient {
public:
    virtual ~IClient() = default;

    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() = 0;
    virtual void subscribe(const std::string& topic) = 0;
    virtual void publish(const std::string& topic, const std::string& payload) = 0;

    using MessageCallback = std::function<void(const std::string&, const std::string&)>;
    using ConnectCallback = std::function<void()>;
    using DisconnectCallback = std::function<void(int)>;

    virtual void setMessageCallback(MessageCallback callback) = 0;
    virtual void setConnectCallback(ConnectCallback callback) = 0;
    virtual void setDisconnectCallback(DisconnectCallback callback) = 0;
};
}
