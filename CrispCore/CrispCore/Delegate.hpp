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
        static ReturnType freeFunctionCaller(void* receiverObject, ParamTypes... args)
        {
            return (*FreeFn)(args...);
        }
    };

    namespace detail
    {
        template <auto MemFn>
        struct MemFnDeductionContext {};

        template <typename ReturnType, typename ClassType, typename... Args, auto (ClassType::* MemFn)(Args...)->ReturnType>
        struct MemFnDeductionContext<MemFn>
        {
            static Delegate<ReturnType, Args...> fromMemberFunction(ClassType* obj)
            {
                return Delegate<ReturnType, Args...>::template fromMemberFunction<MemFn>(obj);
            }
        };

        template <auto FreeFn>
        struct FreeFnDeductionContext {};

        template <typename ReturnType, typename... Args, auto (*FreeFn)(Args...)->ReturnType>
        struct FreeFnDeductionContext<FreeFn>
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
        return detail::MemFnDeductionContext<MemFn>::template fromMemberFunction(obj);
    }

    template<auto FreeFn>
    auto createDelegate() {
        return detail::FreeFnDeductionContext<FreeFn>::template fromFreeFunction();
    }
}