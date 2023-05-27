#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <system_error>
#include <type_traits>
#include <utility>

namespace crisp
{
namespace details
{
template <typename T, typename = void>
struct is_comparable_to_nullptr : std::false_type
{
};

template <typename T>
struct is_comparable_to_nullptr<
    T,
    std::enable_if_t<std::is_convertible<decltype(std::declval<T>() != nullptr), bool>::value>> : std::true_type
{
};

// Resolves to the more efficient of `const T` or `const T&`, in the context of returning a const-qualified value
// of type T.
//
// Copied from cppfront's implementation of the CppCoreGuidelines F.16
// (https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rf-in)
template <typename T>
using value_or_reference_return_t = std::
    conditional_t<sizeof(T) < 2 * sizeof(void*) && std::is_trivially_copy_constructible<T>::value, const T, const T&>;

} // namespace details

//
// GSL.owner: ownership pointers
//
using std::shared_ptr;
using std::unique_ptr;

//
// owner
//
// `gsl::owner<T>` is designed as a safety mechanism for code that must deal directly with raw pointers that own
// memory. Ideally such code should be restricted to the implementation of low-level abstractions. `gsl::owner` can
// also be used as a stepping point in converting legacy code to use more modern RAII constructs, such as smart
// pointers.
//
// T must be a pointer type
// - disallow construction from any type other than pointer type
//
template <class T, class = std::enable_if_t<std::is_pointer<T>::value>>
using owner = T;

//
// not_null
//
// Restricts a pointer or smart pointer to only hold non-null values.
//
// Has zero size overhead over T.
//
// If T is a pointer (i.e. T == U*) then
// - allow construction from U*
// - disallow construction from nullptr_t
// - disallow default construction
// - ensure construction from null U* fails
// - allow implicit conversion to U*
//
template <class T>
class NotNull
{
public:
    static_assert(details::is_comparable_to_nullptr<T>::value, "T cannot be compared to nullptr.");

    template <typename U, typename = std::enable_if_t<std::is_convertible<U, T>::value>>
    constexpr NotNull(U&& u)
        : m_ptr(std::forward<U>(u))
    {
        CRISP_CHECK(m_ptr != nullptr);
    }

    template <typename = std::enable_if_t<!std::is_same<std::nullptr_t, T>::value>>
    constexpr NotNull(T u)
        : m_ptr(std::move(u))
    {
        CRISP_CHECK(m_ptr != nullptr);
    }

    template <typename U, typename = std::enable_if_t<std::is_convertible<U, T>::value>>
    constexpr NotNull(const NotNull<U>& other)
        : NotNull(other.get())
    {
    }

    NotNull(const NotNull& other) = default;
    NotNull& operator=(const NotNull& other) = default;

    constexpr details::value_or_reference_return_t<T> get() const
    {
        return m_ptr;
    }

    constexpr operator T() const
    {
        return get();
    }

    constexpr decltype(auto) operator->() const
    {
        return get();
    }

    constexpr decltype(auto) operator*() const
    {
        return *get();
    }

    // prevents compilation when someone attempts to assign a null pointer constant
    NotNull(std::nullptr_t) = delete;
    NotNull& operator=(std::nullptr_t) = delete;

    // unwanted operators...pointers only point to single objects!
    NotNull& operator++() = delete;
    NotNull& operator--() = delete;
    NotNull operator++(int) = delete;
    NotNull operator--(int) = delete;
    NotNull& operator+=(std::ptrdiff_t) = delete;
    NotNull& operator-=(std::ptrdiff_t) = delete;
    void operator[](std::ptrdiff_t) const = delete;

private:
    T m_ptr;
};

template <class T>
auto makeNotNull(T&& t) noexcept
{
    return NotNull<std::remove_cv_t<std::remove_reference_t<T>>>{std::forward<T>(t)};
}

template <class T, class U>
auto operator==(const NotNull<T>& lhs, const NotNull<U>& rhs) noexcept(noexcept(lhs.get() == rhs.get()))
    -> decltype(lhs.get() == rhs.get())
{
    return lhs.get() == rhs.get();
}

template <class T, class U>
auto operator!=(const NotNull<T>& lhs, const NotNull<U>& rhs) noexcept(noexcept(lhs.get() != rhs.get()))
    -> decltype(lhs.get() != rhs.get())
{
    return lhs.get() != rhs.get();
}

template <class T, class U>
auto operator<(const NotNull<T>& lhs, const NotNull<U>& rhs) noexcept(noexcept(std::less<>{}(lhs.get(), rhs.get())))
    -> decltype(std::less<>{}(lhs.get(), rhs.get()))
{
    return std::less<>{}(lhs.get(), rhs.get());
}

template <class T, class U>
auto operator<=(const NotNull<T>& lhs, const NotNull<U>& rhs) noexcept(
    noexcept(std::less_equal<>{}(lhs.get(), rhs.get()))) -> decltype(std::less_equal<>{}(lhs.get(), rhs.get()))
{
    return std::less_equal<>{}(lhs.get(), rhs.get());
}

template <class T, class U>
auto operator>(const NotNull<T>& lhs, const NotNull<U>& rhs) noexcept(noexcept(std::greater<>{}(lhs.get(), rhs.get())))
    -> decltype(std::greater<>{}(lhs.get(), rhs.get()))
{
    return std::greater<>{}(lhs.get(), rhs.get());
}

template <class T, class U>
auto operator>=(const NotNull<T>& lhs, const NotNull<U>& rhs) noexcept(
    noexcept(std::greater_equal<>{}(lhs.get(), rhs.get()))) -> decltype(std::greater_equal<>{}(lhs.get(), rhs.get()))
{
    return std::greater_equal<>{}(lhs.get(), rhs.get());
}

// more unwanted operators
template <class T, class U>
std::ptrdiff_t operator-(const NotNull<T>&, const NotNull<U>&) = delete;
template <class T>
NotNull<T> operator-(const NotNull<T>&, std::ptrdiff_t) = delete;
template <class T>
NotNull<T> operator+(const NotNull<T>&, std::ptrdiff_t) = delete;
template <class T>
NotNull<T> operator+(std::ptrdiff_t, const NotNull<T>&) = delete;

template <
    class T,
    class U = decltype(std::declval<const T&>().get()),
    bool = std::is_default_constructible<std::hash<U>>::value>
struct NotNullHash
{
    std::size_t operator()(const T& value) const
    {
        return std::hash<U>{}(value.get());
    }
};

template <class T, class U>
struct NotNullHash<T, U, false>
{
    NotNullHash() = delete;
    NotNullHash(const NotNullHash&) = delete;
    NotNullHash& operator=(const NotNullHash&) = delete;
};

} // namespace crisp

namespace std
{
template <class T>
struct hash<crisp::NotNull<T>> : crisp::NotNullHash<crisp::NotNull<T>>
{
};

} // namespace std
