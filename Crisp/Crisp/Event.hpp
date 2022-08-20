#pragma once

#include <algorithm>
#include <functional>
#include <iostream>
#include <set>
#include <variant>
#include <vector>

#include <Crisp/Common/RobinHood.hpp>
#include <Crisp/ConnectionHandler.hpp>
#include <Crisp/Delegate.hpp>

namespace crisp
{
using ConnectionToken = std::size_t;

template <typename... ParamTypes>
class Event
{
public:
    struct Connection
    {
        Connection(ConnectionToken token, std::function<void(ParamTypes...)> callback)
            : key(token)
            , callback(callback)
        {
        }

        Connection(void* obj, std::function<void(ParamTypes...)> callback)
            : key(obj)
            , callback(callback)
        {
        }

        std::variant<ConnectionToken, void*> key;
        std::function<void(ParamTypes...)> callback;
    };

    Event() = default;
    ~Event() = default;

    Event(const Event& other) = delete;
    Event(Event&& other) noexcept = default;
    Event& operator=(const Event& other) = delete;
    Event& operator=(Event&& other) noexcept = default;

    template <auto F, typename ReceiverType = detail::MemFnClassType<F>>
    void subscribe(ReceiverType* obj)
    {
        m_delegates.insert(createDelegate<F>(obj));
    }

    template <auto Fn>
    void subscribe()
    {
        m_delegates.insert(createDelegate<Fn>());
    }

    template <typename FuncType>
    ConnectionToken operator+=(FuncType&& func)
    {
        ConnectionToken token = m_tokenCounter++;
        m_connections.emplace_back(token, std::forward<FuncType>(func));
        return token;
    }

    template <typename FuncType>
    [[nodiscard]] ConnectionHandler subscribe(FuncType&& func)
    {
        ConnectionToken token = m_tokenCounter++;
        m_connections.emplace_back(token, std::forward<FuncType>(func));
        return ConnectionHandler(
            [this, token]
            {
                unsubscribe(token);
            });
    }

    void operator+=(Connection connection)
    {
        m_connections.emplace_back(connection);
    }

    void subscribe(Delegate<void, ParamTypes...> del)
    {
        m_delegates.insert(del);
    }

    void operator+=(Delegate<void, ParamTypes...> del)
    {
        m_delegates.insert(del);
    }

    template <auto F, typename ReceiverType = detail::MemFnClassType<F>>
    void unsubscribe(ReceiverType* obj)
    {
        m_delegates.erase(createDelegate<F>(obj));
    }

    void operator-=(Delegate<void, ParamTypes...> del)
    {
        m_delegates.erase(del);
    }

    void unsubscribe(void* obj)
    {
        auto it = m_delegates.begin();
        while (it != m_delegates.end())
        {
            if (it->isFromObject(obj))
                it = m_delegates.erase(it);
            else
                ++it;
        }

        m_connections.erase(
            std::remove_if(
                m_connections.begin(),
                m_connections.end(),
                [obj](const Connection& conn)
                {
                    return std::visit(
                        [obj](auto&& arg)
                        {
                            using T = std::decay_t<decltype(arg)>;
                            if constexpr (std::is_same_v<T, void*>)
                                return arg == obj;
                            else
                                return false;
                        },
                        conn.key);
                }),
            m_connections.end());
    }

    void unsubscribe(std::variant<ConnectionToken, void*> connectionKey)
    {
        m_connections.erase(
            std::remove_if(
                m_connections.begin(),
                m_connections.end(),
                [&connectionKey](const Connection& conn)
                {
                    return conn.key == connectionKey;
                }),
            m_connections.end());
    }

    void operator()(const ParamTypes&... args) const
    {
        for (auto& delegate : m_delegates)
            delegate(args...);

        for (auto& conn : m_connections)
            conn.callback(args...);
    }

    void clear()
    {
        m_delegates.clear();
        m_connections.clear();
    }

    std::size_t getSubscriberCount() const
    {
        return m_delegates.size() + m_connections.size();
    }

    std::size_t getDelegateCount() const
    {
        return m_delegates.size();
    }

    std::size_t getFunctorCount() const
    {
        return m_connections.size();
    }

private:
    robin_hood::unordered_flat_set<Delegate<void, ParamTypes...>> m_delegates;
    std::vector<Connection> m_connections;
    ConnectionToken m_tokenCounter = 0;
};
} // namespace crisp
