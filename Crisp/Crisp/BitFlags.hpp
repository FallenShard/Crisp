#pragma once

#include <bitset>
#include <iostream>
#include <type_traits>

namespace crisp
{
template <typename Enum>
struct IsBitFlag
{
    static constexpr bool value = false;
};

template <typename EnumBits, typename = typename std::enable_if_t<IsBitFlag<EnumBits>::value>,
    typename MaskType = std::underlying_type_t<EnumBits>>
constexpr MaskType operator|(EnumBits bit1, EnumBits bit2)
{
    return static_cast<MaskType>(bit1) | static_cast<MaskType>(bit2);
}

template <typename EnumBits, typename = typename std::enable_if_t<IsBitFlag<EnumBits>::value>,
    typename MaskType = std::underlying_type_t<EnumBits>>
constexpr MaskType operator|(MaskType bits, EnumBits bit)
{
    return bits | static_cast<MaskType>(bit);
}

template <typename EnumBits, typename = typename std::enable_if_t<IsBitFlag<EnumBits>::value>,
    typename MaskType = std::underlying_type_t<EnumBits>>
class BitFlags
{
public:
    BitFlags()
        : m_mask(MaskType(0))
    {
    }

    BitFlags(EnumBits bits)
        : m_mask(static_cast<MaskType>(bits))
    {
    }

    explicit BitFlags(MaskType mask)
        : m_mask(mask)
    {
    }

    BitFlags& operator|=(const BitFlags& rhs)
    {
        m_mask |= rhs.m_mask;
        return *this;
    }

    BitFlags& operator|=(const EnumBits bit)
    {
        m_mask |= static_cast<MaskType>(bit);
        return *this;
    }

    BitFlags operator|(EnumBits bits)
    {
        return BitFlags(m_mask | static_cast<MaskType>(bits));
    }

    BitFlags operator|(const BitFlags& rhs)
    {
        return BitFlags(m_mask | rhs.m_mask);
    }

    inline bool operator==(const BitFlags& rhs) const
    {
        return m_mask == rhs.m_mask;
    }

    inline bool operator==(const EnumBits bit) const
    {
        return m_mask == static_cast<MaskType>(bit);
    }

    inline bool operator==(const MaskType mask) const
    {
        return m_mask == mask;
    }

    inline bool operator!=(const BitFlags& rhs) const
    {
        return m_mask != rhs.m_mask;
    }

    inline bool operator!=(const EnumBits bit) const
    {
        return m_mask != static_cast<MaskType>(bit);
    }

    inline bool operator!=(const MaskType mask) const
    {
        return m_mask != mask;
    }

    inline bool operator!() const
    {
        return m_mask == 0;
    }

    inline BitFlags operator&(const EnumBits bits) const
    {
        return BitFlags(m_mask & static_cast<MaskType>(bits));
    }

    inline BitFlags operator&(const BitFlags& rhs) const
    {
        return BitFlags(m_mask & rhs.m_mask);
    }

    inline operator bool() const
    {
        return m_mask != 0;
    }

    void print()
    {
        std::cout << std::bitset<sizeof(MaskType)>(m_mask);
    }

    void disable(const EnumBits bit)
    {
        m_mask &= ~(static_cast<MaskType>(bit));
    }

private:
    MaskType m_mask;
};

} // namespace crisp

#define DECLARE_BITFLAG(EnumClass)                                                                                     \
    template <>                                                                                                        \
    struct IsBitFlag<EnumClass>                                                                                        \
    {                                                                                                                  \
        static constexpr bool value = true;                                                                            \
    };                                                                                                                 \
    using EnumClass##Flags = BitFlags<EnumClass>;

#define DECLARE_BITFLAG_IN_NAMESPACE(ns, EnumClass)                                                                    \
    template <>                                                                                                        \
    struct IsBitFlag<ns::EnumClass>                                                                                    \
    {                                                                                                                  \
        static constexpr bool value = true;                                                                            \
    };                                                                                                                 \
    using EnumClass##Flags = BitFlags<ns::EnumClass>;
