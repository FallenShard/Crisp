#pragma once

#include <type_traits>
#include <utility>

namespace crisp {

template <typename Enum>
struct IsBitFlag : public std::false_type {};

template <typename Enum>
concept EnumBitFlagType = std::is_enum_v<Enum> && IsBitFlag<Enum>::value;

template <EnumBitFlagType EnumType>
class BitFlags {
    using MaskType = std::make_unsigned_t<std::underlying_type_t<EnumType>>;

public:
    using BitType = EnumType;
    using Mask = MaskType;

    constexpr BitFlags() noexcept = default;

    // A bit enumerator is intentionally convertible to its corresponding flag set.
    constexpr BitFlags(const EnumType bit) noexcept // NOLINT
        : m_mask(toMask(bit)) {}

    [[nodiscard]] static constexpr BitFlags fromMask(const MaskType mask) noexcept {
        return BitFlags(mask, FromMaskTag{});
    }

    [[nodiscard]] constexpr MaskType getMask() const noexcept {
        return m_mask;
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
        return m_mask == 0;
    }

    [[nodiscard]] constexpr bool contains(const BitFlags flags) const noexcept {
        return containsAll(flags);
    }

    [[nodiscard]] constexpr bool contains(const EnumType bit) const noexcept {
        return containsAll(bit);
    }

    [[nodiscard]] constexpr bool containsAny(const BitFlags flags) const noexcept {
        return (m_mask & flags.m_mask) != 0;
    }

    [[nodiscard]] constexpr bool containsAny(const EnumType bit) const noexcept {
        return containsAny(BitFlags(bit));
    }

    [[nodiscard]] constexpr bool containsAll(const BitFlags flags) const noexcept {
        return (m_mask & flags.m_mask) == flags.m_mask;
    }

    [[nodiscard]] constexpr bool containsAll(const EnumType bits) const noexcept {
        return containsAll(BitFlags(bits));
    }

    constexpr BitFlags& set(const BitFlags flags, const bool enabled = true) noexcept {
        return enabled ? (*this |= flags) : reset(flags);
    }

    constexpr BitFlags& set(const EnumType bit, const bool enabled = true) noexcept {
        return set(BitFlags(bit), enabled);
    }

    constexpr BitFlags& reset(const BitFlags flags) noexcept {
        m_mask &= static_cast<MaskType>(~flags.m_mask);
        return *this;
    }

    constexpr BitFlags& reset(const EnumType bit) noexcept {
        return reset(BitFlags(bit));
    }

    constexpr BitFlags& toggle(const BitFlags flags) noexcept {
        m_mask ^= flags.m_mask;
        return *this;
    }

    constexpr BitFlags& toggle(const EnumType bit) noexcept {
        return toggle(BitFlags(bit));
    }

    constexpr BitFlags& clear() noexcept {
        m_mask = 0;
        return *this;
    }

    constexpr BitFlags& operator|=(const BitFlags rhs) noexcept {
        m_mask |= rhs.m_mask;
        return *this;
    }

    constexpr BitFlags& operator|=(const EnumType bit) noexcept {
        return *this |= BitFlags(bit);
    }

    constexpr BitFlags& operator&=(const BitFlags rhs) noexcept {
        m_mask &= rhs.m_mask;
        return *this;
    }

    constexpr BitFlags& operator&=(const EnumType bits) noexcept {
        return *this &= BitFlags(bits);
    }

    constexpr BitFlags& operator^=(const BitFlags rhs) noexcept {
        m_mask ^= rhs.m_mask;
        return *this;
    }

    constexpr BitFlags& operator^=(const EnumType bits) noexcept {
        return *this ^= BitFlags(bits);
    }

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return !empty();
    }

    [[nodiscard]] constexpr bool operator!() const noexcept {
        return empty();
    }

    friend constexpr bool operator==(const BitFlags&, const BitFlags&) noexcept = default;

    [[nodiscard]] friend constexpr BitFlags operator|(BitFlags lhs, const BitFlags rhs) noexcept {
        lhs |= rhs;
        return lhs;
    }

    [[nodiscard]] friend constexpr BitFlags operator&(BitFlags lhs, const BitFlags rhs) noexcept {
        lhs &= rhs;
        return lhs;
    }

    [[nodiscard]] friend constexpr BitFlags operator^(BitFlags lhs, const BitFlags rhs) noexcept {
        lhs ^= rhs;
        return lhs;
    }

private:
    struct FromMaskTag {};

    constexpr BitFlags(const MaskType mask, FromMaskTag) noexcept
        : m_mask(mask) {}

    [[nodiscard]] static constexpr MaskType toMask(const EnumType bit) noexcept {
        return static_cast<MaskType>(std::to_underlying(bit));
    }

    MaskType m_mask{0};
};

template <EnumBitFlagType EnumType>
[[nodiscard]] constexpr BitFlags<EnumType> operator|(const EnumType lhs, const EnumType rhs) noexcept {
    BitFlags<EnumType> result(lhs);
    result |= rhs;
    return result;
}

template <EnumBitFlagType EnumType>
[[nodiscard]] constexpr BitFlags<EnumType> operator&(const EnumType lhs, const EnumType rhs) noexcept {
    BitFlags<EnumType> result(lhs);
    result &= rhs;
    return result;
}

template <EnumBitFlagType EnumType>
[[nodiscard]] constexpr BitFlags<EnumType> operator^(const EnumType lhs, const EnumType rhs) noexcept {
    BitFlags<EnumType> result(lhs);
    result ^= rhs;
    return result;
}

} // namespace crisp

#define DECLARE_BITFLAG(EnumClass)                                                                                     \
    template <>                                                                                                        \
    struct IsBitFlag<EnumClass> : public std::true_type {};                                                            \
    using EnumClass##Flags = BitFlags<EnumClass>;

#define DECLARE_BITFLAG_IN_NAMESPACE(ns, EnumClass)                                                                    \
    template <>                                                                                                        \
    struct IsBitFlag<ns::EnumClass> : public std::true_type {};                                                        \
    using EnumClass##Flags = BitFlags<ns::EnumClass>;
