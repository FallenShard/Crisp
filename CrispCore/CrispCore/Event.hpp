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
        Event& operator=(const Event& other) = delete;
        Event& operator=(Event&& other) = delete;

        template <auto F, typename ReceiverType = detail::MemFnClassType<F>>
        void subscribe(ReceiverType* obj)
        {
            m_delegates.insert(Delegate<void, ParamTypes...>::fromMemberFunction<F>(obj));
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

        template <auto F, typename ReceiverType = detail::MemFnClassType<F>>
        void unsubscribe(ReceiverType* obj)
        {
            m_delegates.erase(Delegate<void, ParamTypes...>::fromMemberFunction<F>(obj));
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
