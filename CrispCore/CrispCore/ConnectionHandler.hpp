#pragma once

#include <functional>
#include <memory>

namespace crisp
{
    struct ConnectionHandler
    {
    public:
        inline ~ConnectionHandler()
        {
            if (disconnectCallback)
                (*disconnectCallback)();
        }

        inline ConnectionHandler(ConnectionHandler&& other) noexcept : disconnectCallback(std::move(other.disconnectCallback)) {}

        inline ConnectionHandler& operator=(ConnectionHandler&& other) noexcept {
            if (this == &other)
                return *this;

            disconnectCallback = std::move(other.disconnectCallback);
            other.disconnectCallback = nullptr;
            return *this;
        }

    private:
        template <typename ...ParamTypes> friend class Event;

        inline ConnectionHandler(std::function<void()> disconnectCallback) : disconnectCallback(
            std::make_unique<std::function<void()>>(disconnectCallback)) {}

        std::unique_ptr<std::function<void()>> disconnectCallback;
    };
}