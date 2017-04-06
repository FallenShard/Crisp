#pragma once

#include <type_traits>

namespace vesper
{
    template <typename EnumBits, typename MaskType = std::underlying_type<EnumBits>::type>
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

        BitFlags(MaskType mask)
            : m_mask(mask)
        {
        }

        BitFlags(const BitFlags& rhs)
            : m_mask(rhs.m_mask)
        {
        }

        BitFlags& operator=(const BitFlags& rhs)
        {
            m_mask = rhs.m_mask;
            return *this;
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

        inline bool operator&(const EnumBits bits) const
        {
            return m_mask & static_cast<MaskType>(bits);
        }

    private:
        MaskType m_mask;
    };
}
