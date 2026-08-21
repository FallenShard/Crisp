#pragma once

#include <concepts>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include <Crisp/Core/Result.hpp>

namespace crisp::detail {

// MSVC resolves a zero-argument call to the formatting overload into fmt's consteval constructor
// (format_string is a non-deduced alias, so an empty pack leaves it viable) and rejects it with
// C7595. The tag makes the two overloads differ by arity, which no longer depends on deduction.
struct CheckMessageTag {};

inline std::string makeCheckMessage(CheckMessageTag) {
    return {};
}

template <typename... Args>
std::string makeCheckMessage(CheckMessageTag, fmt::format_string<Args...> formatString, Args&&... args) {
    return fmt::format(formatString, std::forward<Args>(args)...);
}

template <typename T>
std::string describeCheckOperand(const T& value) {
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::nullptr_t>) {
        return "nullptr";
    } else if constexpr (fmt::formattable<T>) {
        return fmt::format("{}", value);
    } else if constexpr (std::is_pointer_v<T> && std::is_object_v<std::remove_pointer_t<T>>) {
        // fmt refuses object pointers to keep them from decaying into something printable by accident.
        return fmt::format("{}", static_cast<const void*>(value));
    } else {
        return "<unformattable>";
    }
}

template <typename L, typename R>
std::string describeOperands(const char* lhsName, const L& lhs, const char* rhsName, const R& rhs) {
    return fmt::format("\n  {} = {}\n  {} = {}", lhsName, describeCheckOperand(lhs), rhsName, describeCheckOperand(rhs));
}

template <typename A, typename B, typename C>
std::string describeOperands(
    const char* aName, const A& a, const char* bName, const B& b, const char* cName, const C& c) {
    return fmt::format(
        "\n  {} = {}\n  {} = {}\n  {} = {}",
        aName,
        describeCheckOperand(a),
        bName,
        describeCheckOperand(b),
        cName,
        describeCheckOperand(c));
}

[[noreturn]] inline void failCheck(
    const char* condition,
    const std::string& operands,
    const std::string& message,
    const std::source_location& loc) noexcept {
    spdlog::critical(
        "Check failed: {}{}{}\nFile: {}\n({}:{}) -- Function: `{}`",
        condition,
        operands,
        message.empty() ? std::string{} : fmt::format("\nMessage: {}", message),
        loc.file_name(),
        loc.line(),
        loc.column(),
        loc.function_name());
    std::abort();
}

template <typename T>
inline constexpr bool kIsSignednessComparable =
    std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char> && !std::is_same_v<T, char8_t> &&
    !std::is_same_v<T, char16_t> && !std::is_same_v<T, char32_t> && !std::is_same_v<T, wchar_t>;

template <typename L, typename R>
concept SignednessComparable = kIsSignednessComparable<L> && kIsSignednessComparable<R>;

template <typename L, typename R>
inline constexpr bool kIsExactFloatComparison = std::is_floating_point_v<L> || std::is_floating_point_v<R>;

#define CRISP_DETAIL_REJECT_FLOAT_EQUALITY                                                                             \
    static_assert(                                                                                                     \
        !kIsExactFloatComparison<L, R>,                                                                                \
        "Exact floating point equality is almost never the intended check. Use CRISP_CHECK_NEAR.")

// Integer comparisons route through std::cmp_* so that a negative value never compares as huge
// against an unsigned bound, and so call sites need no sign-silencing casts.
#define CRISP_DETAIL_DEFINE_COMPARE(name, op, safeOp, ...)                                                             \
    template <typename L, typename R>                                                                                  \
    constexpr bool name(const L& lhs, const R& rhs) {                                                                  \
        __VA_ARGS__;                                                                                                   \
        if constexpr (SignednessComparable<L, R>) {                                                                    \
            return std::safeOp(lhs, rhs);                                                                              \
        } else {                                                                                                       \
            return lhs op rhs;                                                                                         \
        }                                                                                                              \
    }

CRISP_DETAIL_DEFINE_COMPARE(checkEq, ==, cmp_equal, CRISP_DETAIL_REJECT_FLOAT_EQUALITY)
CRISP_DETAIL_DEFINE_COMPARE(checkNe, !=, cmp_not_equal, CRISP_DETAIL_REJECT_FLOAT_EQUALITY)
CRISP_DETAIL_DEFINE_COMPARE(checkLt, <, cmp_less)
CRISP_DETAIL_DEFINE_COMPARE(checkLe, <=, cmp_less_equal)
CRISP_DETAIL_DEFINE_COMPARE(checkGt, >, cmp_greater)
CRISP_DETAIL_DEFINE_COMPARE(checkGe, >=, cmp_greater_equal)

#undef CRISP_DETAIL_DEFINE_COMPARE
#undef CRISP_DETAIL_REJECT_FLOAT_EQUALITY

template <typename T, typename L, typename U>
constexpr bool checkInRangeHalfOpen(const T& value, const L& lower, const U& upper) {
    return checkGe(value, lower) && checkLt(value, upper);
}

template <typename T, typename L, typename U>
constexpr bool checkInRangeClosed(const T& value, const L& lower, const U& upper) {
    return checkGe(value, lower) && checkLe(value, upper);
}

template <typename T, typename U, typename E>
constexpr bool checkNear(const T& value, const U& target, const E& tolerance) {
    return (value > target ? value - target : target - value) <= tolerance;
}

// Declared, never defined: the disabled CRISP_DEV_* macros call it inside sizeof, which type-checks
// and name-checks every argument without evaluating any of them.
template <typename... Args>
int discardCheckArgs(const Args&...);

} // namespace crisp::detail

namespace crisp {

// Narrows only after proving the value survives the round trip. In a constant expression an
// out-of-range value fails to compile, because the failure path is not constexpr.
template <std::integral To, std::integral From>
[[nodiscard]] constexpr To checkedCast(
    const From value, const std::source_location& loc = std::source_location::current()) {
    static_assert(!std::is_same_v<To, bool>, "Narrowing to bool discards the value. Compare explicitly instead.");

    if (!detail::checkGe(value, std::numeric_limits<To>::min()) ||
        !detail::checkLe(value, std::numeric_limits<To>::max())) [[unlikely]] {
        detail::failCheck(
            "checked cast is out of the target type's range",
            detail::describeOperands(
                "value", value, "min", std::numeric_limits<To>::min(), "max", std::numeric_limits<To>::max()),
            {},
            loc);
    }

    return static_cast<To>(value);
}

} // namespace crisp

// clang-format off
#define CRISP_STRINGIFY_BINARY_OP(a, op, b) #a " " #op " " #b
// clang-format on

#define CRISP_DETAIL_FAIL(conditionString, operands, ...)                                                              \
    crisp::detail::failCheck(                                                                                          \
        conditionString,                                                                                               \
        operands,                                                                                                      \
        crisp::detail::makeCheckMessage(crisp::detail::CheckMessageTag {} __VA_OPT__(, ) __VA_ARGS__),                 \
        std::source_location::current())

// The message is formatted and the source location materialized on the failing branch only.
#define CRISP_DETAIL_CHECK(expr, ...)                                                                                  \
    do {                                                                                                               \
        if (!(expr)) [[unlikely]] {                                                                                    \
            CRISP_DETAIL_FAIL(#expr, {}, __VA_ARGS__);                                                                 \
        }                                                                                                              \
    } while (false)

// Operands bind once, so a side-effecting argument is not evaluated twice and the failure can report
// the values that actually compared false.
#define CRISP_DETAIL_CHECK_BINARY(lhs, rhs, compareFn, conditionString, lhsName, rhsName, ...)                         \
    do {                                                                                                               \
        const auto& crispCheckLhs = (lhs);                                                                             \
        const auto& crispCheckRhs = (rhs);                                                                             \
        if (!crisp::detail::compareFn(crispCheckLhs, crispCheckRhs)) [[unlikely]] {                                    \
            CRISP_DETAIL_FAIL(                                                                                         \
                conditionString,                                                                                       \
                crisp::detail::describeOperands(lhsName, crispCheckLhs, rhsName, crispCheckRhs),                       \
                __VA_ARGS__);                                                                                          \
        }                                                                                                              \
    } while (false)

#define CRISP_DETAIL_CHECK_TERNARY(a, b, c, compareFn, conditionString, aName, bName, cName, ...)                      \
    do {                                                                                                               \
        const auto& crispCheckA = (a);                                                                                 \
        const auto& crispCheckB = (b);                                                                                 \
        const auto& crispCheckC = (c);                                                                                 \
        if (!crisp::detail::compareFn(crispCheckA, crispCheckB, crispCheckC)) [[unlikely]] {                           \
            CRISP_DETAIL_FAIL(                                                                                         \
                conditionString,                                                                                       \
                crisp::detail::describeOperands(aName, crispCheckA, bName, crispCheckB, cName, crispCheckC),           \
                __VA_ARGS__);                                                                                          \
        }                                                                                                              \
    } while (false)

#define CRISP_CHECK(expr, ...) CRISP_DETAIL_CHECK(expr, __VA_ARGS__)

#define CRISP_CHECK_EQ(expr, right, ...)                                                                               \
    CRISP_DETAIL_CHECK_BINARY(                                                                                         \
        expr, right, checkEq, CRISP_STRINGIFY_BINARY_OP(expr, ==, right), "lhs", "rhs", __VA_ARGS__)
#define CRISP_CHECK_NE(expr, right, ...)                                                                               \
    CRISP_DETAIL_CHECK_BINARY(                                                                                         \
        expr, right, checkNe, CRISP_STRINGIFY_BINARY_OP(expr, !=, right), "lhs", "rhs", __VA_ARGS__)
#define CRISP_CHECK_LT(expr, right, ...)                                                                               \
    CRISP_DETAIL_CHECK_BINARY(                                                                                         \
        expr, right, checkLt, CRISP_STRINGIFY_BINARY_OP(expr, <, right), "lhs", "rhs", __VA_ARGS__)
#define CRISP_CHECK_LE(expr, right, ...)                                                                               \
    CRISP_DETAIL_CHECK_BINARY(                                                                                         \
        expr, right, checkLe, CRISP_STRINGIFY_BINARY_OP(expr, <=, right), "lhs", "rhs", __VA_ARGS__)
#define CRISP_CHECK_GT(expr, right, ...)                                                                               \
    CRISP_DETAIL_CHECK_BINARY(                                                                                         \
        expr, right, checkGt, CRISP_STRINGIFY_BINARY_OP(expr, >, right), "lhs", "rhs", __VA_ARGS__)
#define CRISP_CHECK_GE(expr, right, ...)                                                                               \
    CRISP_DETAIL_CHECK_BINARY(                                                                                         \
        expr, right, checkGe, CRISP_STRINGIFY_BINARY_OP(expr, >=, right), "lhs", "rhs", __VA_ARGS__)

#define CRISP_CHECK_GE_LT(expr, left, right, ...)                                                                      \
    CRISP_DETAIL_CHECK_TERNARY(                                                                                        \
        expr, left, right, checkInRangeHalfOpen, #expr " in [" #left ", " #right ")", "value", "lower", "upper", __VA_ARGS__)
#define CRISP_CHECK_GE_LE(expr, left, right, ...)                                                                      \
    CRISP_DETAIL_CHECK_TERNARY(                                                                                        \
        expr, left, right, checkInRangeClosed, #expr " in [" #left ", " #right "]", "value", "lower", "upper", __VA_ARGS__)
#define CRISP_CHECK_NEAR(expr, right, tolerance, ...)                                                                  \
    CRISP_DETAIL_CHECK_TERNARY(                                                                                        \
        expr,                                                                                                          \
        right,                                                                                                         \
        tolerance,                                                                                                     \
        checkNear,                                                                                                     \
        "|" #expr " - " #right "| <= " #tolerance,                                                                     \
        "value",                                                                                                       \
        "target",                                                                                                      \
        "tolerance",                                                                                                   \
        __VA_ARGS__)

#define CRISP_CHECK_NOT_NULL(expr, ...) CRISP_DETAIL_CHECK((expr) != nullptr, __VA_ARGS__)

#define CRISP_CHECK_INDEX(index, container, ...)                                                                       \
    CRISP_DETAIL_CHECK_BINARY(                                                                                         \
        index, std::size(container), checkLt, #index " < size(" #container ")", "index", "size", __VA_ARGS__)
#define CRISP_CHECK_SIZE_EQ(lhs, rhs, ...)                                                                             \
    CRISP_DETAIL_CHECK_BINARY(                                                                                         \
        std::size(lhs), std::size(rhs), checkEq, "size(" #lhs ") == size(" #rhs ")", "lhs size", "rhs size", __VA_ARGS__)

// Development-only mirrors of every check above. Same semantics, compiled out of release builds --
// for hot paths where the always-on tier costs more than the invariant is worth.
#ifdef _DEBUG
#define CRISP_DEV_CHECK(...) CRISP_CHECK(__VA_ARGS__)
#define CRISP_DEV_CHECK_EQ(...) CRISP_CHECK_EQ(__VA_ARGS__)
#define CRISP_DEV_CHECK_NE(...) CRISP_CHECK_NE(__VA_ARGS__)
#define CRISP_DEV_CHECK_LT(...) CRISP_CHECK_LT(__VA_ARGS__)
#define CRISP_DEV_CHECK_LE(...) CRISP_CHECK_LE(__VA_ARGS__)
#define CRISP_DEV_CHECK_GT(...) CRISP_CHECK_GT(__VA_ARGS__)
#define CRISP_DEV_CHECK_GE(...) CRISP_CHECK_GE(__VA_ARGS__)
#define CRISP_DEV_CHECK_GE_LT(...) CRISP_CHECK_GE_LT(__VA_ARGS__)
#define CRISP_DEV_CHECK_GE_LE(...) CRISP_CHECK_GE_LE(__VA_ARGS__)
#define CRISP_DEV_CHECK_NEAR(...) CRISP_CHECK_NEAR(__VA_ARGS__)
#define CRISP_DEV_CHECK_NOT_NULL(...) CRISP_CHECK_NOT_NULL(__VA_ARGS__)
#define CRISP_DEV_CHECK_INDEX(...) CRISP_CHECK_INDEX(__VA_ARGS__)
#define CRISP_DEV_CHECK_SIZE_EQ(...) CRISP_CHECK_SIZE_EQ(__VA_ARGS__)
#else
// The arguments stay name- and type-checked in an unevaluated context, so a release build cannot break
// a development-only check into something that no longer compiles.
#define CRISP_DETAIL_DISCARD(...) ((void)sizeof(crisp::detail::discardCheckArgs(__VA_ARGS__)))
#define CRISP_DEV_CHECK(...) CRISP_DETAIL_DISCARD(__VA_ARGS__)
#define CRISP_DEV_CHECK_EQ(...) CRISP_DETAIL_DISCARD(__VA_ARGS__)
#define CRISP_DEV_CHECK_NE(...) CRISP_DETAIL_DISCARD(__VA_ARGS__)
#define CRISP_DEV_CHECK_LT(...) CRISP_DETAIL_DISCARD(__VA_ARGS__)
#define CRISP_DEV_CHECK_LE(...) CRISP_DETAIL_DISCARD(__VA_ARGS__)
#define CRISP_DEV_CHECK_GT(...) CRISP_DETAIL_DISCARD(__VA_ARGS__)
#define CRISP_DEV_CHECK_GE(...) CRISP_DETAIL_DISCARD(__VA_ARGS__)
#define CRISP_DEV_CHECK_GE_LT(...) CRISP_DETAIL_DISCARD(__VA_ARGS__)
#define CRISP_DEV_CHECK_GE_LE(...) CRISP_DETAIL_DISCARD(__VA_ARGS__)
#define CRISP_DEV_CHECK_NEAR(...) CRISP_DETAIL_DISCARD(__VA_ARGS__)
#define CRISP_DEV_CHECK_NOT_NULL(...) CRISP_DETAIL_DISCARD(__VA_ARGS__)
#define CRISP_DEV_CHECK_INDEX(...) CRISP_DETAIL_DISCARD(__VA_ARGS__)
#define CRISP_DEV_CHECK_SIZE_EQ(...) CRISP_DETAIL_DISCARD(__VA_ARGS__)
#endif

#define CRISP_FATAL(...)                                                                                               \
    {                                                                                                                  \
        spdlog::critical(__VA_ARGS__);                                                                                 \
        std::terminate();                                                                                              \
    }
