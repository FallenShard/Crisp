#pragma once

#include <set>
#include <vector>
#include <functional>

#include "Delegate.hpp"

namespace crisp
{
    // Variadic template magic
    template<typename ReturnType, typename... ParamTypes>
    class Event
    {
    public:
        Event() = default;
        ~Event() = default;

        Event(const Event& other) = delete;
        Event<ReturnType, ParamTypes...>& operator=(const Event& other) = delete;
        Event<ReturnType, ParamTypes...>& operator=(Event&& other) = delete;

        template<typename T, ReturnType(T::*callbackMethod)(ParamTypes...)>
        void subscribe(T* obj)
        {
            m_delegates.insert(Delegate<ReturnType, ParamTypes...>::fromFunction<T, callbackMethod>(obj));
        }

        template<ReturnType(*callbackMethod)(ParamTypes...)>
        void subscribe()
        {
            m_delegates.insert(Delegate<ReturnType, ParamTypes...>::fromStaticFunction<callbackMethod>());
        }

        template <typename FuncType>
        void subscribe(FuncType&& func)
        {
            m_functors.emplace_back(std::forward<FuncType>(func));
        }

        template<typename T>
        void subscribe(Delegate<ReturnType, ParamTypes...> del)
        {
            m_delegates.insert(del);
        }

        template<typename T, ReturnType(T::*callbackMethod)(ParamTypes...)>
        void unsubscribe(T* obj)
        {
            m_delegates.erase(Delegate<ReturnType, ParamTypes...>::fromFunction<T, callbackMethod>(obj));
        }

        void operator()(ParamTypes... args)
        {
            for (auto& delegate : m_delegates)
                delegate(args...);

            for (auto& func : m_functors)
                func(args...);
        }

    private:
        std::set<Delegate<ReturnType, ParamTypes...>> m_delegates;
        std::vector<std::function<ReturnType(ParamTypes...)>> m_functors;
    };
}
