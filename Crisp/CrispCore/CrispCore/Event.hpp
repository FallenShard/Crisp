#pragma once

#include <set>
#include <vector>
#include <functional>

#include "Delegate.hpp"

namespace crisp
{
    template<typename ...ParamTypes>
    class Event
    {
    public:
        Event() = default;
        ~Event() = default;

        Event(const Event& other) = delete;
        Event<void, ParamTypes...>& operator=(const Event& other) = delete;
        Event<void, ParamTypes...>& operator=(Event&& other) = delete;

        template<typename T, void(T::*callbackMethod)(ParamTypes...)>
        void subscribe(T* obj)
        {
            m_delegates.insert(Delegate<void, ParamTypes...>::fromFunction<T, callbackMethod>(obj));
        }

        template<void(*callbackMethod)(ParamTypes...)>
        void subscribe()
        {
            m_delegates.insert(Delegate<void, ParamTypes...>::fromStaticFunction<callbackMethod>());
        }

        template <typename FuncType>
        void subscribe(FuncType&& func)
        {
            m_functors.emplace_back(std::forward<FuncType>(func));
        }

        template <typename FuncType>
        void operator+=(FuncType&& func)
        {
            m_functors.emplace_back(std::forward<FuncType>(func));
        }

        template<typename T>
        void subscribe(Delegate<void, ParamTypes...> del)
        {
            m_delegates.insert(del);
        }

        template<typename T, void(T::*callbackMethod)(ParamTypes...)>
        void unsubscribe(T* obj)
        {
            m_delegates.erase(Delegate<void, ParamTypes...>::fromFunction<T, callbackMethod>(obj));
        }

        void operator()(ParamTypes... args)
        {
            for (auto& delegate : m_delegates)
                delegate(args...);

            for (auto& func : m_functors)
                func(args...);
        }

        void clear()
        {
            m_delegates.clear();
            m_functors.clear();
        }

    private:
        std::set<Delegate<void, ParamTypes...>>         m_delegates;
        std::vector<std::function<void(ParamTypes...)>> m_functors;
    };
}
