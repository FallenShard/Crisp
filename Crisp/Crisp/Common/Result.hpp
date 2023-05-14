#pragma once

#include <Crisp/Common/Logger.hpp>

#include <expected>
#include <source_location>

namespace crisp
{
namespace detail
{
template <typename T>
class Unexpected
{
public:
    constexpr explicit Unexpected(T&& val)
        : m_value(val)
    {
    }

    std::string&& value()
    {
        return std::move(m_value);
    }

private:
    T m_value;
};

struct LocationFormatString
{
    fmt::string_view str;
    std::source_location loc;

    LocationFormatString(const char* str = "", const std::source_location& loc = std::source_location::current())
        : str(str)
        , loc(loc)
    {
    }
};
} // namespace detail

template <typename T = void>
class Result
{
public:
    constexpr Result(const Result&) = delete;
    constexpr Result(Result&&) = default;

    constexpr Result(const T& value)
        : m_expected(value)
    {
    }

    constexpr Result(T&& value)
        : m_expected(std::move(value))
    {
    }

    constexpr Result(std::unexpected<std::string>&& unexp)
        : m_expected(std::move(unexp))
    {
    }

    constexpr T&& unwrap()
    {
        if (!m_expected)
        {
            spdlog::critical(m_expected.error());
            std::exit(1);
        }

        return std::move(m_expected).value();
    }

    constexpr T&& extract()
    {
        return std::move(m_expected).value();
    }

    constexpr std::string getError()
    {
        if (m_expected.has_value())
        {
            spdlog::critical("Failed to extract error!");
            std::exit(1);
        }

        return std::move(m_expected.error());
    }

    constexpr const std::string& getError() const
    {
        if (m_expected.has_value())
        {
            spdlog::critical("Failed to get error!");
            std::exit(1);
        }

        return m_expected.error();
    }

    constexpr bool hasValue() const
    {
        return m_expected.has_value();
    }

    constexpr operator bool() const
    {
        return m_expected.has_value();
    }

private:
    std::expected<T, std::string> m_expected;
};

template <>
class Result<void>
{
public:
    constexpr Result() = default;
    constexpr Result(const Result&) = delete;
    constexpr Result(Result&&) = default;

    constexpr Result(std::unexpected<std::string>&& unexp)
        : m_expected(std::move(unexp))
    {
    }

    constexpr void unwrap()
    {
        if (!m_expected)
        {
            spdlog::critical(m_expected.error());
            std::exit(1);
        }
    }

    constexpr bool isValid()
    {
        return m_expected.has_value();
    }

    ~Result()
    {
        unwrap();
    }

private:
    std::expected<void, std::string> m_expected;
};

template <typename... Args>
std::unexpected<std::string> resultError(detail::LocationFormatString&& formatString, Args&&... args)
{
    std::string errorMessage = fmt::format(fmt::runtime(formatString.str), std::forward<Args>(args)...);
    spdlog::error(
        "File: {}\n[Function: {}][Line {}, Column {}] Error:\n {}",
        formatString.loc.file_name(),
        formatString.loc.function_name(),
        formatString.loc.line(),
        formatString.loc.column(),
        errorMessage);
    return std::unexpected<std::string>(std::move(errorMessage));
}
} // namespace crisp