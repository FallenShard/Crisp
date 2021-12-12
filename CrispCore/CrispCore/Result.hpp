#pragma once

#include <CrispCore/Logger.hpp>

#include <source_location>


namespace crisp
{
	namespace detail
	{
		template <typename T>
		class Unexpected
		{
		public:
			constexpr explicit Unexpected(T&& val) : m_value(val) {}

			std::string&& value() { return std::move(m_value); }

		private:
			T m_value;
		};

		struct LocationFormatString
		{
			fmt::string_view str;
			std::source_location loc;

			LocationFormatString(const char* str, const std::source_location& loc = std::source_location::current()) : str(str), loc(loc) {}
		};
	}


	template <typename T = void>
	class Result
	{
	public:
		constexpr Result(const Result&) = delete;
		constexpr Result(Result&&) = default;
		constexpr Result(const T& val) : m_value(val) {}
		constexpr Result(T&& val) : m_value(std::move(val)) {}
		constexpr Result(detail::Unexpected<std::string>&& unexp) : m_error(std::move(unexp.value())) {}

		constexpr T unwrap()
		{
			if (!m_value)
			{
				spdlog::critical(*m_error);
				std::exit(1);
			}
				

			return std::move(*m_value);
		}

		constexpr std::string getError()
		{
			if (!m_error)
			{
				spdlog::critical("Failed to extract error!");
				std::exit(1);
			}


			return std::move(*m_error);
		}

		constexpr bool hasValue() const
		{
			return m_value.has_value();
		}

	private:
		std::optional<T> m_value;
		std::optional<std::string> m_error;
	};

	template <>
	class Result<void>
	{
	public:
		constexpr Result(const Result&) = delete;
		constexpr Result(Result&&) = default;
		constexpr Result() {}
		constexpr Result(detail::Unexpected<std::string>&& unexp) : m_error(std::move(unexp.value())) {}

		constexpr void unwrap()
		{
			if (m_error)
			{
				spdlog::critical(*m_error);
				std::exit(1);
			}
		}

		~Result()
		{
			unwrap();
		}

	private:
		std::optional<std::string> m_error;
	};

	

	template <typename ...Args>
	detail::Unexpected<std::string> resultError(detail::LocationFormatString&& formatString, Args&&... args)
	{
		spdlog::critical("File: {}\n[Function: {}][Line {}, Column {}]", formatString.loc.file_name(), formatString.loc.function_name(), formatString.loc.line(), formatString.loc.column());
		return detail::Unexpected<std::string>(fmt::format(formatString.str, std::forward<Args>(args)...));
	}
}