#pragma once

#include <functional>
#include <memory>

namespace crisp
{
struct ConnectionHandler
{
public:
    ~ConnectionHandler()
    {
        if (disconnectCallback)
        {
            disconnectCallback();
        }
    }

    ConnectionHandler(const ConnectionHandler&) = delete;
    ConnectionHandler& operator=(const ConnectionHandler&) = delete;

    ConnectionHandler(ConnectionHandler&& other) noexcept = default;
    ConnectionHandler& operator=(ConnectionHandler&& other) noexcept = default;

private:
    template <typename... ParamTypes>
    friend class Event;

    explicit ConnectionHandler(std::function<void()> disconnectCallback)
        : disconnectCallback(std::move(disconnectCallback))
    {
    }

    std::function<void()> disconnectCallback;
};
} // namespace crisp