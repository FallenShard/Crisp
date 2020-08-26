#pragma once

namespace crisp
{
    namespace detail
    {
        template<typename ReturnType, typename ClassType, typename... Args>
        ClassType deduceMemberFunctionClass(ReturnType(ClassType::*)(Args...)) {}

        template<auto MemFn>
        using MemFnClassType = decltype(deduceMemberFunctionClass(MemFn));
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

        template<ReturnType(*SMethod)(ParamTypes...)>
        static Delegate fromStaticFunction()
        {
            return Delegate(nullptr, &staticFunctionCaller<SMethod>);
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

        Delegate(void* receiverObject, CallbackType function)
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

        template<ReturnType(*SMethod)(ParamTypes...)>
        static ReturnType staticFunctionCaller(void* receiverObject, ParamTypes... args)
        {
            return (*SMethod)(args...);
        }
    };
}