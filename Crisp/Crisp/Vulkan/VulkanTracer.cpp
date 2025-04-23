#include <Crisp/Vulkan/VulkanTracer.hpp>

namespace crisp {
namespace {

CRISP_MAKE_LOGGER_ST("VulkanTracer");

std::vector<gsl::not_null<VulkanTracingContext*>> traceContexts;
uint32_t contextIdx{0};

} // namespace

VulkanTracingContext::VulkanTracingContext(const VulkanDevice& device, const VulkanPhysicalDevice& physicalDevice)
    : m_device(&device)
    , m_queryPool(VK_NULL_HANDLE)
    , m_maxQueryCount(1 << 17)
    , m_timestampPeriod(physicalDevice.getLimits().timestampPeriod)
    , m_referenceTimepoint{0}
    , m_first(0)
    , m_count(0)
    , m_retrievedQueries(m_maxQueryCount)
    , m_resolvedEvents(0) {
    VkQueryPoolCreateInfo queryPoolInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolInfo.queryCount = m_maxQueryCount;
    vkCreateQueryPool(m_device->getHandle(), &queryPoolInfo, nullptr, &m_queryPool);

    vkResetQueryPool(m_device->getHandle(), m_queryPool, 0, m_maxQueryCount);

    device.getGeneralQueue().submitAndWait([this](const VkCommandBuffer cmdBuffer) {
        vkCmdWriteTimestamp2(cmdBuffer, VK_PIPELINE_STAGE_2_NONE, m_queryPool, 0);
    });
    vkGetQueryPoolResults(
        m_device->getHandle(),
        m_queryPool,
        0,
        1,
        sizeof(uint64_t),
        &m_referenceTimepoint,
        sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    const auto cpuTimestampNs = std::chrono::steady_clock::now().time_since_epoch().count();
    vkResetQueryPool(m_device->getHandle(), m_queryPool, 0, m_maxQueryCount);

    const auto gpuTimestampNs = static_cast<int64_t>(static_cast<double>(m_referenceTimepoint) * m_timestampPeriod);

    m_referenceTimepoint = gpuTimestampNs - cpuTimestampNs;
}

VulkanTracingContext::~VulkanTracingContext() {
    vkDestroyQueryPool(m_device->getHandle(), m_queryPool, nullptr);
}

void VulkanTracingContext::retrieveResults() {
    if (m_count > 0) {
        const auto status = vkGetQueryPoolResults(
            m_device->getHandle(),
            m_queryPool,
            m_first,
            m_count,
            m_count * sizeof(uint64_t),
            m_retrievedQueries.data(),
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT);
        if (status == VK_NOT_READY) {
            return;
        }
        vkResetQueryPool(m_device->getHandle(), m_queryPool, 0, m_maxQueryCount);

        const size_t prevEventOffset = m_events.size() - m_count;
        for (size_t i = 0; i < m_count; ++i) {
            auto& event = m_events[prevEventOffset + i];
            event.timestamp =
                static_cast<uint64_t>(static_cast<double>(m_retrievedQueries[event.timestamp]) * m_timestampPeriod) -
                m_referenceTimepoint;
        }

        m_resolvedEvents += m_count;
        m_first = 0;
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