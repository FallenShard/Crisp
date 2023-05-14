#pragma once

#include <vector>

#include <Crisp/Core/HashMap.hpp>

namespace crisp
{
template <typename H, typename V>
class SlotMap
{
public:
    using ValueVector = std::vector<V>;

    explicit SlotMap(const size_t reserveCount)
    {
        m_values.reserve(reserveCount);
        m_valueIdxToHandleIdx.reserve(reserveCount);
        m_handles.reserve(reserveCount);
    }

    ValueVector::iterator begin()
    {
        return m_values.begin();
    }

    ValueVector::const_iterator cbegin() const
    {
        return m_values.cbegin();
    }

    ValueVector::iterator end()
    {
        return m_values.end();
    }

    ValueVector::const_iterator cend() const
    {
        return m_values.cend();
    }

    V& at(HandleType handle);
    const V& at(HandleType handle) const;

    V& operator[](HandleType handle);
    const V& operator[](HandleType handle) const;

    template <typename... Args>
    HandleType emplace(Args... args)
    {
        return insert(V{std::forward<Args>(args...)});
    }

    bool freeListEmpty() const
    {
        return true;
    }

    template <typename ValueT>
    H insert(ValueT&& value)
    {
        H handle{};

        if (freeListEmpty())
        {
            handle.index = static_cast<uint16_t>(m_handles.size());
            handle.generation = 1;
            m_handles.push_back(handle);
        }
        else
        {
            const uint32_t firstFreeIndex = m_handleListFront;
            HandleType& freeHandle = m_handles.at(firstFreeIndex);
            m_handleListFront = freeHandle.index;

            if (freeListEmpty())
            {
                m_handleListBack = m_handleListFront;
            }

            // freeHandle.free = 0;
            freeHandle.index = m_values.size();
            handle = freeHandle;
            handle.index = firstFreeIndex;
            handle.generation = freeHandle.generation;
        }

        m_values.push_back(std::forward<ValueT>(value));
        m_valueToSparseIndex.push_back(handle.index);
        return handle;
    }

    size_t erase(const H handle)
    {
        if (!isValid(handle))
        {
            return 0;
        }

        H innerIdx = m_handles[handle.index];
        uint16_t innerIndex = innerIdx.index;

        ++innerIdx.generation;
        innerIdx.index = 0xFFFFFFFF;

        m_handles[handle.index] = innerIdx;

        if (innerIndex != m_values.size() - 1)
        {
            std::swap(m_values[innerIndex], m_values.back());
            std::swap(m_valueIdxToHandleIdx[innerIndex], m_valueIdxToHandleIdx.back());

            m_handles[m_valueIdxToHandleIdx[innerIndex]].index = innerIndex;
        }

        m_values.pop_back();
        m_valueIdxToHandleIdx.pop_back();
        return 1;
    }

private:
    std::vector<V> m_values;
    std::vector<uint32_t> m_valueIdxToHandleIdx;
    std::vector<HandleType> m_handles;

    uint16_t m_handleListFront;
    uint16_t m_handleListBack;
};
} // namespace crisp
