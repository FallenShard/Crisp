#pragma once

#include <Crisp/Core/ChromeEventTracer.hpp>

#include <fstream>

namespace crisp {

CpuTracerContext::CpuTracerContext() = default;

void CpuTracerContext::addEvent(ScopeEvent&& event) {
    m_events.push_back(event);
}

const std::vector<ScopeEvent>& CpuTracerContext::getTracedEvents() const {
    return m_events;
}

namespace detail {
CpuTracerContext CpuContext;

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

void serializeTracedEvents(const std::filesystem::path& outputFile) {
    std::ofstream output(outputFile);
    output << "{\"traceEvents\":[\n";

    const auto& ctx = detail::getCpuContext();

    std::vector<uint32_t> stack;
    const auto& tracedEvents = ctx.getTracedEvents();
    for (uint32_t i = 0; i < tracedEvents.size(); ++i) {
        auto& event = tracedEvents[i];
        if (event.type == ScopeEventType::Begin) {
            stack.push_back(i);
        } else {
            const auto& beginEvent = tracedEvents[stack.back()];
            stack.pop_back();

            const auto beginUs = static_cast<double>(beginEvent.timestamp) / 1000.0;
            const auto durationUs = static_cast<double>(event.timestamp - beginEvent.timestamp) / 1000.0;
            const std::string_view comma = i == tracedEvents.size() - 1 ? "\n" : ",\n";
            output << fmt::format(
                R"({{"name":{:?}, "ph":"X", "ts":{}, "dur":{}, "pid":{}}}{})",
                beginEvent.name.literal,
                beginUs,
                durationUs,
                1,
                comma);
        }
    }
    output << "],\n";
    output << R"("meta_user":"FallenShard","meta_cpu_count":"16"})";
}

} // namespace crisp