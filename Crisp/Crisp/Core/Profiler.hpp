#pragma once

#include <chrono>

#include <Crisp/Core/Logger.hpp>

namespace crisp {

class LiteralWrapper {
public:
    template <size_t N>
    consteval LiteralWrapper(const char (&literal)[N]) // NOLINT
        : literal(literal) {}

    const char* literal{nullptr};

    bool operator==(const std::string_view& other) const {
        return other == std::string_view(literal);
    }
};

class ScopeProfiler {
public:
    explicit ScopeProfiler(LiteralWrapper scopeName)
        : m_timePoint(std::chrono::high_resolution_clock::now())
        , m_scopeName(scopeName) {}

    ~ScopeProfiler() {
        const double elapsed =
            std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - m_timePoint).count();
        spdlog::info("{} took {:.3f} seconds.", m_scopeName.literal, elapsed);
    }

    ScopeProfiler(const ScopeProfiler&) = delete;
    ScopeProfiler& operator=(const ScopeProfiler&) = delete;

    ScopeProfiler(ScopeProfiler&&) = delete;
    ScopeProfiler& operator=(ScopeProfiler&&) = delete;

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_timePoint;
    LiteralWrapper m_scopeName;
};
} // namespace crisp