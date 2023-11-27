#pragma once

#pragma warning(push)
#pragma warning(disable : 26819) // Fallthrough.
#pragma warning(disable : 26800) // Use of moved-from.
#include <robin_hood.h>
#pragma warning(pop)

#include <ankerl/unordered_dense.h>

namespace crisp {
template <typename Key, typename Value>
using HashMap = robin_hood::unordered_map<Key, Value>;

template <typename Key>
using HashSet = robin_hood::unordered_set<Key>;

template <typename Key, typename Value, typename Hash = ankerl::unordered_dense::hash<Key>>
using FlatHashMap = ankerl::unordered_dense::map<Key, Value, Hash>;

template <typename Key>
using FlatHashSet = ankerl::unordered_dense::set<Key>;
} // namespace crisp
