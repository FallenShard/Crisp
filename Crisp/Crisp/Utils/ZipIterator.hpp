#pragma once

#include <iterator>

namespace crisp {
template <
    typename T,
    typename U,
    typename TIter = decltype(std::begin(std::declval<T>())),
    typename UIter = decltype(std::begin(std::declval<U>())),
    typename = decltype(std::end(std::declval<T>())),
    typename = decltype(std::end(std::declval<U>()))>
constexpr auto zipIterator(T&& iterableA, U&& iterableB) {
    struct iterator {
        TIter iterA;
        UIter iterB;

        bool operator!=(const iterator& other) const {
            return iterA != other.iterA;
        }

        void operator++() {
            ++iterA;
            ++iterB;
        }

        auto operator*() const {
            return std::tie(*iterA, *iterB);
        }
    };

    struct iterable_wrapper {
        T iterableA;
        U iterableB;

        auto begin() {
            return iterator{std::begin(iterableA), std::begin(iterableB)};
        }

        auto end() {
            return iterator{std::end(iterableA), std::end(iterableB)};
        }
    };

    return iterable_wrapper{std::forward<T>(iterableA), std::forward<U>(iterableB)};
}

} // namespace crisp