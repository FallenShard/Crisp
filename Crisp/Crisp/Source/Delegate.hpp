#pragma once

namespace crisp
{
    // Variadic template magic
    template<typename ReturnType, typename... ParamTypes>
    class Delegate
    {
        using CallbackType = ReturnType(*)(void* receiverObject, ParamTypes...);

    public:
        Delegate(void* receiverObject, CallbackType function)
            : m_receiverObject(receiverObject)
            , m_callbackFunction(function)
        {
        }

        // Creates a delegate from specified method
        template<typename T, ReturnType(T::*TMethod)(ParamTypes...)>
        static Delegate fromFunction(T* receiverObject)
        {
            return Delegate(receiverObject, &methodCaller<T, TMethod>);
        }

        template<ReturnType(*SMethod)(ParamTypes...)>
        static Delegate fromStaticFunction()
        {
            return Delegate(nullptr, &staticMethodCaller<SMethod>);
        }

        // Executes the stored callback
        ReturnType operator()(ParamTypes... args) const
        {
            return (*m_callbackFunction)(m_receiverObject, args...);
        }

        // For storage in containers as a key
        bool operator<(const Delegate& other) const
        {
            if (m_receiverObject < other.m_receiverObject)
                return true;
            else if (m_receiverObject == other.m_receiverObject)
                return m_callbackFunction < other.m_callbackFunction;
            else
                return false;
        }

    private:
        void*        m_receiverObject;
        CallbackType m_callbackFunction;

        // Where the magic happens
        template<typename T, ReturnType(T::*TMethod)(ParamTypes...)>
        static ReturnType methodCaller(void* receiverObject, ParamTypes... args)
        {
            return (static_cast<T*>(receiverObject)->*TMethod)(args...);
        }

        template<ReturnType(*SMethod)(ParamTypes...)>
        static ReturnType staticMethodCaller(void* receiverObject, ParamTypes... args)
        {
            return (*SMethod)(args...);
        }
    };
}