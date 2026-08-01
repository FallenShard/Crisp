#pragma once

#include <expected>
#include <source_location>
#include <type_traits>

#include <Crisp/Core/Logger.hpp>

namespace crisp {
namespace detail {
struct LocationFormatString {
    std::string str;
    std::source_location loc;

    LocationFormatString( // NOLINT
        std::string&& str = "",
        const std::source_location& loc = std::source_location::current())
        : str(std::move(str))
        , loc(loc) {}
};
} // namespace detail

template <typename T = void>
class Result {
public:
    constexpr Result(const T& value) // NOLINT
        : m_expected(value) {}

    constexpr Result(T&& value) // NOLINT
        : m_expected(std::move(value)) {}

    constexpr Result(std::unexpected<std::string>&& unexp) // NOLINT
        : m_expected(std::move(unexp)) {}

    constexpr T&& unwrap() {
        if (!m_expected) {
            spdlog::critical(m_expected.error());
            std::exit(1);
        }

        return std::move(m_expected).value();
    }

    constexpr T&& extract() {
        return std::move(m_expected).value();
    }

    constexpr std::string getError() {
        if (m_expected.has_value()) {
            spdlog::critical("Failed to extract error!");
            std::exit(1);
        }

        return std::move(m_expected.error());
    }

    constexpr const std::string& getError() const {
        if (m_expected.has_value()) {
            spdlog::critical("Failed to get error!");
            std::exit(1);
        }

        return m_expected.error();
    }

    constexpr bool hasValue() const {
        return m_expected.has_value();
    }

    constexpr const T& operator*() const {
        return *m_expected;
    }

    constexpr T& operator*() {
        return *m_expected;
    }

    constexpr const T* operator->() const {
        return m_expected.operator->();
    }

    constexpr operator bool() const // NOLINT
    {
        return m_expected.has_value();
    }

private:
    std::expected<T, std::string> m_expected;
};

template <>
class [[nodiscard]] Result<void> // NOLINT
{
public:
    constexpr Result() {} // NOLINT

    constexpr Result(std::unexpected<std::string>&& unexp) // NOLINT
        : m_expected(std::move(unexp)) {}

    constexpr void unwrap() {
        if (!m_expected) {
            spdlog::critical(m_expected.error());
            std::exit(1);
        }
    }

    constexpr bool isValid() const {
        return m_expected.has_value();
    }

    constexpr std::string getError() {
        if (m_expected.has_value()) {
            spdlog::critical("Failed to extract error!");
            std::exit(1);
        }

        return std::move(m_expected.error());
    }

    constexpr const std::string& getError() const {
        if (m_expected.has_value()) {
            spdlog::critical("Failed to get error!");
            std::exit(1);
        }

        return m_expected.error();
    }

private:
    std::expected<void, std::string> m_expected;
};

inline constexpr Result<void> kResultSuccess;

namespace detail {

template <typename... Args>
std::unexpected<std::string> resultErrorImpl(detail::LocationFormatString&& formatString, Args&&... args) { // NOLINT
    spdlog::error(
        "File: {}\n[Function: {}][Line {}, Column {}] Error:\n {}",
        formatString.loc.file_name(),
        formatString.loc.function_name(),
        formatString.loc.line(),
        formatString.loc.column(),
        formatString.str);
    return std::unexpected<std::string>(std::move(formatString.str));
}

template <typename BindingProbe, typename T>
decltype(auto) extractForDeclaration(Result<T>& result) {
    if constexpr (std::is_invocable_v<BindingProbe, T&&>) {
        return std::move(result).extract();
    } else {
        return *result;
    }
}

inline Result<> tryResult(const std::source_location&, Result<>&& result) {
    return std::move(result);
}

template <typename... Args>
Result<> tryResult(
    const std::source_location& location,
    const Result<>& result,
    fmt::format_string<Args...> formatString,
    Args&&... args) {
    if (result.isValid()) {
        return {};
    }
    return resultErrorImpl(LocationFormatString(fmt::format(formatString, std::forward<Args>(args)...), location));
}

template <typename Output, typename T>
Result<> tryResult(const std::source_location&, Output& output, Result<T>&& result) {
    if (!result) {
        return std::unexpected<std::string>(std::move(result).getError());
    }
    output = std::move(result).extract();
    return {};
}

template <typename Output, typename T, typename... Args>
Result<> tryResult(
    const std::source_location& location,
    Output& output,
    Result<T>&& result,
    fmt::format_string<Args...> formatString,
    Args&&... args) {
    if (!result) {
        return resultErrorImpl(LocationFormatString(fmt::format(formatString, std::forward<Args>(args)...), location));
    }
    output = std::move(result).extract();
    return {};
}
} // namespace detail

#define resultError(...) detail::resultErrorImpl(__VA_OPT__(fmt::format(__VA_ARGS__)))

#define CRISP_DETAIL_CONCAT_IMPL(lhs, rhs) lhs##rhs
#define CRISP_DETAIL_CONCAT(lhs, rhs) CRISP_DETAIL_CONCAT_IMPL(lhs, rhs)
#define CRISP_DETAIL_SECOND(first, second, ...) second
#define CRISP_DETAIL_PROBE() ~, 1,
#define CRISP_DETAIL_IS_PROBE(...) CRISP_DETAIL_SECOND(__VA_ARGS__, 0)
#define CRISP_DETAIL_IS_DECLARATION_auto CRISP_DETAIL_PROBE()
#define CRISP_DETAIL_IS_DECLARATION_const CRISP_DETAIL_PROBE()
#define CRISP_DETAIL_IS_DECLARATION(value)                                                                             \
    CRISP_DETAIL_IS_PROBE(CRISP_DETAIL_CONCAT(CRISP_DETAIL_IS_DECLARATION_, value))
#define CRISP_DETAIL_IF_0(ifTrue, ifFalse) ifFalse
#define CRISP_DETAIL_IF_1(ifTrue, ifFalse) ifTrue
#define CRISP_DETAIL_IF(value) CRISP_DETAIL_CONCAT(CRISP_DETAIL_IF_, value)
#define CRISP_DETAIL_SELECT_FIRST(first, ...) first
#define CRISP_DETAIL_UNIQUE_NAME(name) CRISP_DETAIL_CONCAT(name, __LINE__)

#define CRISP_DETAIL_RETURN_ERROR_NO_LOG(result) return std::unexpected<std::string>(std::move(result).getError())
#define CRISP_DETAIL_RETURN_ERROR_LOG(result, ...) return resultError(__VA_ARGS__)
#define CRISP_DETAIL_RETURN_ERROR(result, ...)                                                                         \
    CRISP_DETAIL_SELECT_FIRST(__VA_OPT__(CRISP_DETAIL_RETURN_ERROR_LOG, ) CRISP_DETAIL_RETURN_ERROR_NO_LOG)            \
    (result __VA_OPT__(, ) __VA_ARGS__)

#define CRISP_DETAIL_TRY_VALUE(lhs, resultExpr, ...)                                                                   \
    auto CRISP_DETAIL_UNIQUE_NAME(crispTryResult) = (resultExpr);                                                      \
    if (!CRISP_DETAIL_UNIQUE_NAME(crispTryResult)) {                                                                   \
        CRISP_DETAIL_RETURN_ERROR(CRISP_DETAIL_UNIQUE_NAME(crispTryResult) __VA_OPT__(, ) __VA_ARGS__);                \
    }                                                                                                                  \
    lhs = detail::extractForDeclaration<decltype([]([[maybe_unused]] lhs) {})>(                                        \
        CRISP_DETAIL_UNIQUE_NAME(crispTryResult)) // NOLINT

#define CRISP_DETAIL_TRY_EXPRESSION(first, ...)                                                                        \
    do {                                                                                                               \
        auto crispTryResult = detail::tryResult(std::source_location::current(), first __VA_OPT__(, ) __VA_ARGS__);    \
        if (!crispTryResult.isValid()) {                                                                               \
            CRISP_DETAIL_RETURN_ERROR_NO_LOG(crispTryResult);                                                          \
        }                                                                                                              \
    } while (false) // NOLINT

#define CRISP_TRY(first, ...)                                                                                          \
    CRISP_DETAIL_IF(CRISP_DETAIL_IS_DECLARATION(first))(CRISP_DETAIL_TRY_VALUE, CRISP_DETAIL_TRY_EXPRESSION)(          \
        first __VA_OPT__(, ) __VA_ARGS__)

} // namespace crisp
