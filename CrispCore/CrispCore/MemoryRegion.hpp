#pragma once

namespace crisp
{
class MemoryRegion
{
public:
    inline MemoryRegion(void* ptr, size_t size)
        : m_ptr(ptr)
        , m_size(size)
    {
    }

    void* getPtr() const
    {
        return m_ptr;
    }

    size_t getSize() const
    {
        return m_size;
    }

private:
    void* m_ptr;
    size_t m_size;
};

} // namespace crisp