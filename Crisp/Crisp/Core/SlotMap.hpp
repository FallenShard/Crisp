#pragma once

#include <Crisp/Core/HashMap.hpp>

#include <vector>

namespace crisp
{
template <typename IntegerType, size_t IdBits, size_t GenerationBits>
struct HandleType
{
    IntegerType id : IdBits;
    IntegerType generation : GenerationBits;
    // static_assert(sizeof(HandleType) == sizeof(IntegerType));
};

using Handle = HandleType<uint32_t, 20, 12>;

template <typename ValueType>
class SlotMap
{
public:
    using HandleType = HandleType<uint32_t, 20, 12>;

    using ValueVector = std::vector<ValueType>;
    using HandleVector = std::vector<HandleType>;

    explicit SlotMap(const size_t reserveCount = 1024)
    {
        m_values.reserve(reserveCount);
    }

    size_t size() const
    {
        return m_values.size();
    }

    bool empty() const
    {
        return m_values.empty();
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

    bool contains(HandleType handle) const
    {
        if (handle.id >= m_denseToSparseIndexMapping.size())
        {
            return false;
        }

        const auto handleIdx = m_denseToSparseIndexMapping[handle.id];
        return m_handles[handleIdx].generation == handle.generation;
    }

    ValueType& at(HandleType handle)
    {
        return m_values.at(handle.id);
    }

    const ValueType& at(HandleType handle) const
    {
        return m_values.at(handle.id);
    }

    ValueType& operator[](HandleType handle)
    {
        return m_values[handle.id];
    }

    const ValueType& operator[](HandleType handle) const
    {
        return m_values[handle.id];
    }

    template <typename... Args>
    HandleType emplace(Args... args)
    {
        return insert(ValueType{std::forward<Args>(args)...});
    }

    template <typename ValueT>
    HandleType insert(ValueT&& value)
    {
        if (m_freeListStart == kInvalidFreeListIndex)
        {
            HandleType handle;
            handle.id = static_cast<uint32_t>(m_values.size());
            handle.generation = 0;

            m_handles.push_back(handle);
            m_values.push_back(std::forward<ValueT>(value));
            m_denseToSparseIndexMapping.push_back(handle.id);

            return handle;
        }

        HandleType handle;

        // Retrieve the next free list start before we lose it.
        const auto nextFreeIndex = m_handles[m_freeListStart].id;

        handle.id = static_cast<uint32_t>(m_values.size());
        handle.generation = m_handles[m_freeListStart].generation;

        // Assign the new handle at the slot.
        m_handles[m_freeListStart] = handle;
        m_values.push_back(std::forward<ValueT>(value));
        m_denseToSparseIndexMapping.push_back(m_freeListStart);

        handle.id = m_freeListStart;

        if (m_handles.size() == m_values.size())
        {
            m_freeListStart = kInvalidFreeListIndex;
        }
        else
        {
            m_freeListStart = nextFreeIndex;
        }

        return handle;
    }

    bool erase(const HandleType handle)
    {
        const auto handleIdx = handle.id;
        if (m_handles[handleIdx].generation != handle.generation)
        {
            return false;
        }

        const auto innerIdx = m_handles[handleIdx].id;
        const auto lastItemHandleIdx = m_denseToSparseIndexMapping.back();

        std::swap(m_denseToSparseIndexMapping.back(), m_denseToSparseIndexMapping.at(innerIdx));
        std::swap(m_values.back(), m_values.at(innerIdx));

        // Update the last item's inner index.
        m_handles[lastItemHandleIdx].id = innerIdx;

        // Enlist the handle slot as free.
        m_handles[handleIdx].generation++;
        if (m_freeListStart == kInvalidFreeListIndex)
        {
            m_freeListStart = handleIdx;
        }
        else
        {
            m_handles[handleIdx].id = m_freeListStart;
            m_freeListStart = handleIdx;
        }

        m_denseToSparseIndexMapping.pop_back();
        m_values.pop_back();

        return true;
    }

private:
    ValueVector m_values;
    HandleVector m_handles;
    std::vector<uint32_t> m_denseToSparseIndexMapping;

    static constexpr uint32_t kInvalidFreeListIndex{~0u};
    uint32_t m_freeListStart{kInvalidFreeListIndex};
};
} // namespace crisp
