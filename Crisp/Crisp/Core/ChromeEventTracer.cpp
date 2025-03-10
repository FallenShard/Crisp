#pragma once

#include <Crisp/Core/ChromeEventTracer.hpp>

namespace crisp {

CpuTracerContext::CpuTracerContext() = default;

void CpuTracerContext::addEvent(const ScopeEvent& event) {
    m_events.push_back(event);
}

const std::vector<ScopeEvent>& CpuTracerContext::getTracedEvents() const {
    return m_events;
}

namespace detail {
namespace {
CpuTracerContext CpuContext;
} // namespace

CpuTracerContext& getCpuContext() {
    return CpuContext;
}
} // namespace detail

CpuTracerScope::CpuTracerScope(CpuTracerContext& context, LiteralWrapper scopeName)
    : m_context(context)
    , m_scopeName(scopeName) {
    m_context.addEvent({
        .timestamp = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()),
        .name = scopeName,
        .type = ScopeEventType::Begin,
    });
}

CpuTracerScope::~CpuTracerScope() {
    m_context.addEvent({
        .timestamp = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()),
        .name = m_scopeName,
        .type = ScopeEventType::End,
    });
}

} // namespace crisp