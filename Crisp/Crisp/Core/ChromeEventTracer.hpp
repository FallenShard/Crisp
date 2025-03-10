#pragma once

#include <filesystem>

#include <Crisp/Core/Profiler.hpp>

namespace crisp {
enum class ScopeEventType : uint8_t { Begin, End };

struct ScopeEvent {
    uint64_t timestamp{};
    LiteralWrapper name;
    ScopeEventType type{ScopeEventType::Begin};
};

class CpuTracerContext {
public:
    CpuTracerContext();

    void addEvent(const ScopeEvent& event);

    const std::vector<ScopeEvent>& getTracedEvents() const;

private:
    std::vector<ScopeEvent> m_events;
};

namespace detail {
CpuTracerContext& getCpuContext();
} // namespace detail

class CpuTracerScope {
public:
    CpuTracerScope(CpuTracerContext& context, LiteralWrapper scopeName);
    ~CpuTracerScope();

    CpuTracerScope(const CpuTracerScope&) = delete;
    CpuTracerScope operator=(const CpuTracerScope&) = delete;

    CpuTracerScope(CpuTracerScope&&) = delete;
    CpuTracerScope operator=(CpuTracerScope&&) = delete;

private:
    CpuTracerContext& m_context;
    LiteralWrapper m_scopeName;
};

#define CRISP_CONCAT_IMPL(x, y) x##y
#define CRISP_CONCATENATE(x, y) CRISP_CONCAT_IMPL(x, y)
#define CRISP_FORCE_EXPAND(x) x

#define CRISP_TRACE_SCOPE(scopeName)                                                                                   \
    CpuTracerScope CRISP_CONCATENATE(scope, __LINE__)(detail::getCpuContext(), scopeName);

} // namespace crisp