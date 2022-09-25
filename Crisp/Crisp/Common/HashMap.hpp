#pragma once

#pragma warning(push)
#pragma warning(disable : 26819) // Fallthrough.
#pragma warning(disable : 26800) // Use of moved-from.
#include <robin_hood.h>
#pragma warning(pop)

namespace crisp
{
template <typename Key, typename Value>
using FlatHashMap = robin_hood::unordered_flat_map<Key, Value>;

template <typename Key>
using FlatHashSet = robin_hood::unordered_flat_set<Key>;

template <typename Key, typename Value>
using HashMap = robin_hood::unordered_map<Key, Value>;

template <typename Key>
using HashSet = robin_hood::unordered_set<Key>;
} // namespace crisp
