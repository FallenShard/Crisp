#pragma once

#include <type_traits>

namespace crisp
{
    template <typename Enum>
    struct IsBitFlag
    {
        static constexpr bool value = false;
    };

    template <typename EnumBits, typename = std::enable_if_t<IsBitFlag<EnumBits>::value>, typename MaskType = std::underlying_type<EnumBits>::type>
    constexpr MaskType operator|(EnumBits bit1, EnumBits bit2)
    {
        return static_cast<MaskType>(bit1) | static_cast<MaskType>(bit2);
    }

    template <typename EnumBits, typename = std::enable_if_t<IsBitFlag<EnumBits>::value>, typename MaskType = std::underlying_type<EnumBits>::type>
    MaskType operator|(MaskType bits, EnumBits bit)
    {
        return bits | static_cast<MaskType>(bit);
    }

    template <typename EnumBits, typename = std::enable_if_t<IsBitFlag<EnumBits>::value>, typename MaskType = std::underlying_type<EnumBits>::type>
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

        bool operator==(const BitFlags& rhs)
        {
            return m_mask == rhs.m_mask;
        }

        inline bool operator&(const EnumBits bits) const
        {
            return m_mask & static_cast<MaskType>(bits);
        }

        inline bool operator&(const BitFlags& rhs) const
        {
            return m_mask & rhs.m_mask;
        }

        void print()
        {
            std::cout << m_mask << '\n';
        }

    private:
        MaskType m_mask;
    };
}