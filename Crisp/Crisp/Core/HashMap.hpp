#pragma once

#pragma warning(push)
#pragma warning(disable : 26819) // Fallthrough.
#pragma warning(disable : 26800) // Use of moved-from.
#include <robin_hood.h>
#pragma warning(pop)

#include <ankerl/unordered_dense.h>

namespace crisp {
namespace detail {
struct TransparentStringHash {
    using is_transparent = void; // Enable heterogeneous overloads.
    using is_avalanching = void; // Mark class as high quality avalanching hash.

    [[nodiscard]] auto operator()(const std::string_view str) const noexcept -> uint64_t {
        return ankerl::unordered_dense::hash<std::string_view>{}(str);
    }
};
} // namespace detail

template <typename Key, typename Value>
using HashMap = robin_hood::unordered_map<Key, Value>;

template <typename Key>
using HashSet = robin_hood::unordered_set<Key>;

template <typename Key, typename Value, typename Hash = ankerl::unordered_dense::hash<Key>>
using FlatHashMap = ankerl::unordered_dense::map<Key, Value, Hash>;

template <typename Key>
using FlatHashSet = ankerl::unordered_dense::set<Key>;

template <typename Value>
using FlatStringHashMap =
    ankerl::unordered_dense::map<std::string, Value, detail::TransparentStringHash, std::equal_to<>>;

} // namespace crisp
