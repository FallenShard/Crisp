#include <Crisp/Vulkan/VulkanTracer.hpp>

#include <array>

namespace crisp {
namespace {

CRISP_MAKE_LOGGER_ST("VulkanTracer");

std::vector<gsl::not_null<VulkanTracingContext*>> traceContexts;
uint32_t contextIdx{0};

} // namespace

VulkanTracingContext::VulkanTracingContext(const VulkanDevice& device)
    : m_queryPool(device, device.getGeneralQueue(), kMaxQueryCount, "Vulkan Tracing Queries")
    , m_referenceTimepoint{0}
    , m_count(0)
    , m_retrievedQueries(kMaxQueryCount)
    , m_resolvedEvents(0) {
    device.getGeneralQueue().submitAndWait([this](const VkCommandBuffer cmdBuffer) {
        m_queryPool.writeTimestamp(cmdBuffer, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0);
    });
    std::array<uint64_t, 1> referenceTimestamp{};
    m_queryPool.getResultsAndWait(referenceTimestamp);
    const auto cpuTimestampNs = std::chrono::steady_clock::now().time_since_epoch().count();
    m_queryPool.reset();

    const auto gpuTimestampNs = static_cast<int64_t>(m_queryPool.toNanoseconds(referenceTimestamp.front()));

    m_referenceTimepoint = gpuTimestampNs - cpuTimestampNs;
}

void VulkanTracingContext::retrieveResults() {
    if (m_count > 0) {
        auto queryResults = std::span(m_retrievedQueries).first(m_count);
        if (!m_queryPool.tryGetResults(queryResults)) {
            return;
        }
        m_queryPool.reset();

        const size_t prevEventOffset = m_events.size() - m_count;
        for (size_t i = 0; i < m_count; ++i) {
            auto& event = m_events[prevEventOffset + i];
            event.timestamp =
                static_cast<uint64_t>(m_queryPool.toNanoseconds(m_retrievedQueries[event.timestamp])) -
                m_referenceTimepoint;
        }

        m_resolvedEvents += m_count;
        m_count = 0;
    }
}

namespace detail {

void setTraceContexts(const std::span<std::unique_ptr<VulkanTracingContext>> contexts) {
    traceContexts.reserve(contexts.size());
    for (const auto& context : contexts) {
        traceContexts.emplace_back(context.get());
    }
}

std::span<gsl::not_null<VulkanTracingContext*>> getTraceContexts() {
    return traceContexts;
}

VulkanTracingContext* getTraceContext() {
    if (traceContexts.empty()) {
        return nullptr;
    }
    return traceContexts[contextIdx];
}

void setTraceContextIndex(const uint32_t index) {
    contextIdx = index;
}

} // namespace detail

} // namespace crisp
