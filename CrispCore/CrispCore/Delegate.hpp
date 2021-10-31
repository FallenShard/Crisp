#pragma once

namespace crisp
{
    namespace detail
    {
        template<typename ReturnType, typename ClassType, typename... Args>
        ClassType deduceMemberFunctionClassType(ReturnType(ClassType::*)(Args...)) {}

        template<auto MemFn>
        using MemFnClassType = decltype(deduceMemberFunctionClassType(MemFn));
    }

    template<typename ReturnType, typename... ParamTypes>
    class Delegate
    {
        using CallbackType = ReturnType(*)(void*, ParamTypes...);

    public:
        // Creates a delegate from specified method
        template <auto MemFn, typename ReceiverType = detail::MemFnClassType<MemFn>>
        static Delegate fromMemberFunction(ReceiverType* receiverObject)
        {
            return Delegate(receiverObject, &memberFunctionCaller<MemFn, ReceiverType>);
        }

        template<auto FreeFn>
        static Delegate fromStaticFunction()
        {
            return Delegate(nullptr, &freeFunctionCaller<FreeFn>);
        }

        // Executes the stored callback
        ReturnType operator()(ParamTypes... args) const
        {
            return (*m_callbackFunction)(m_receiverObject, args...);
        }

        // For storage in containers as a key
        inline bool operator<(const Delegate& other) const
        {
            if (m_receiverObject == other.m_receiverObject)
                return m_callbackFunction < other.m_callbackFunction;

            return m_receiverObject < other.m_receiverObject;
        }

        inline bool operator==(const Delegate& other) const
        {
            return m_receiverObject == other.m_receiverObject && m_callbackFunction == other.m_callbackFunction;
        }

        inline bool operator!=(const Delegate& other) const
        {
            return !operator==(other);
        }

        inline bool isFromObject(void* obj) const
        {
            return m_receiverObject == obj;
        }

    private:
        void*        m_receiverObject;
        CallbackType m_callbackFunction;

        friend std::hash<Delegate>;

        explicit Delegate(void* receiverObject, CallbackType function)
            : m_receiverObject(receiverObject)
            , m_callbackFunction(function)
        {
        }

        // Where the magic happens
        template<auto MemFn, typename ReceiverType>
        static ReturnType memberFunctionCaller(void* receiverObject, ParamTypes... args)
        {
            return (static_cast<ReceiverType*>(receiverObject)->*MemFn)(args...);
        }

        template<auto FreeFn>
        static ReturnType freeFunctionCaller(void*, ParamTypes... args)
        {
            return (*FreeFn)(args...);
        }
    };

    namespace detail
    {
        template <auto MemFn>
        struct FuncDeductionContext {};

        template <typename ReturnType, typename ClassType, typename... Args, auto (ClassType::* MemFn)(Args...)->ReturnType>
        struct FuncDeductionContext<MemFn>
        {
            static Delegate<ReturnType, Args...> fromMemberFunction(ClassType* obj)
            {
                return Delegate<ReturnType, Args...>::template fromMemberFunction<MemFn>(obj);
            }
        };

        template <typename ReturnType, typename... Args, auto (*FreeFn)(Args...)->ReturnType>
        struct FuncDeductionContext<FreeFn>
        {
            static Delegate<ReturnType, Args...> fromFreeFunction()
            {
                return Delegate<ReturnType, Args...>::template fromStaticFunction<FreeFn>();
            }

            ReturnType operator()(Args... args) const
            {
                return FreeFn(args...);
            }
        };
    }

    template <auto MemFn, typename ReceiverType = detail::MemFnClassType<MemFn>>
    auto createDelegate(ReceiverType* obj) {
        return detail::FuncDeductionContext<MemFn>::template fromMemberFunction(obj);
    }

    template<auto FreeFn>
    auto createDelegate() {
        return detail::FuncDeductionContext<FreeFn>::template fromFreeFunction();
    }
}

namespace std
{
    template<typename ReturnType, typename... ParamTypes>
    struct hash<crisp::Delegate<ReturnType, ParamTypes...>>
    {
        std::size_t operator()(const crisp::Delegate<ReturnType, ParamTypes...>& delegate) const
        {
            return std::hash<decltype(delegate.m_receiverObject)>()(delegate.m_receiverObject)
                ^ std::hash<decltype(delegate.m_callbackFunction)>()(delegate.m_callbackFunction);
        }
    };
}