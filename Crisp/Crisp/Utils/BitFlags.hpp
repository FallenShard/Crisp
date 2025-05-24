#pragma once

#include <bitset>
#include <iostream>
#include <type_traits>

namespace crisp {
template <typename Enum>
struct IsBitFlag : public std::false_type {};

template <typename Enum>
concept EnumBitFlagType = IsBitFlag<Enum>::value;

template <EnumBitFlagType EnumType, typename MaskType = std::underlying_type_t<EnumType>>
constexpr MaskType operator|(EnumType bit1, EnumType bit2) {
    return static_cast<MaskType>(bit1) | static_cast<MaskType>(bit2);
}

template <EnumBitFlagType EnumType, typename MaskType = std::underlying_type_t<EnumType>>
constexpr MaskType operator|(std::underlying_type_t<EnumType> bits, EnumType bit) {
    return bits | static_cast<MaskType>(bit);
}

template <EnumBitFlagType EnumType, typename MaskType = std::underlying_type_t<EnumType>>
class BitFlags {
public:
    BitFlags()
        : m_mask(MaskType(0)) {}

    BitFlags(EnumType bits) // NOLINT
        : m_mask(static_cast<MaskType>(bits)) {}

    explicit BitFlags(MaskType mask)
        : m_mask(mask) {}

    BitFlags& operator|=(const BitFlags& rhs) {
        m_mask |= rhs.m_mask;
        return *this;
    }

    BitFlags& operator|=(const EnumType bit) {
        m_mask |= static_cast<MaskType>(bit);
        return *this;
    }

    BitFlags operator|(EnumType bits) {
        return BitFlags(m_mask | static_cast<MaskType>(bits));
    }

    BitFlags operator|(const BitFlags& rhs) {
        return BitFlags(m_mask | rhs.m_mask);
    }

    bool operator==(const BitFlags& rhs) const {
        return m_mask == rhs.m_mask;
    }

    bool operator==(const EnumType bit) const {
        return m_mask == static_cast<MaskType>(bit);
    }

    bool operator==(const MaskType mask) const {
        return m_mask == mask;
    }

    bool operator!=(const BitFlags& rhs) const {
        return m_mask != rhs.m_mask;
    }

    bool operator!=(const EnumType bit) const {
        return m_mask != static_cast<MaskType>(bit);
    }

    bool operator!=(const MaskType mask) const {
        return m_mask != mask;
    }

    bool operator!() const {
        return m_mask == 0;
    }

    BitFlags& operator&=(const BitFlags& rhs) {
        m_mask &= rhs.m_mask;
        return *this;
    }

    BitFlags& operator&=(const EnumType bits) {
        m_mask &= static_cast<MaskType>(bits);
        return *this;
    }

    BitFlags operator&(const EnumType bits) const {
        return BitFlags(m_mask & static_cast<MaskType>(bits));
    }

    BitFlags operator&(const BitFlags& rhs) const {
        return BitFlags(m_mask & rhs.m_mask);
    }

    /*implicit*/ operator bool() const // NOLINT
    {
        return m_mask != 0;
    }

    MaskType getMask() const {
        return m_mask;
    }

    void print() {
        std::cout << std::bitset<sizeof(MaskType)>(m_mask);
    }

    void disable(const EnumType bit) {
        m_mask &= ~(static_cast<MaskType>(bit));
    }

    void disable(const BitFlags& rhs) {
        m_mask &= ~rhs.m_mask;
    }

private:
    MaskType m_mask;
};

} // namespace crisp

#define DECLARE_BITFLAG(EnumClass)                                                                                     \
    template <>                                                                                                        \
    struct IsBitFlag<EnumClass> : public std::true_type {};                                                            \
    using EnumClass##Flags = BitFlags<EnumClass>;

#define DECLARE_BITFLAG_IN_NAMESPACE(ns, EnumClass)                                                                    \
    template <>                                                                                                        \
    struct IsBitFlag<ns::EnumClass> : public std::true_type {};                                                        \
    using EnumClass##Flags = BitFlags<ns::EnumClass>;
