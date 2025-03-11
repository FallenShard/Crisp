#pragma once

#include <span>
#include <vector>

#include <gsl/pointers>

#include <Crisp/Core/ChromeEventTracer.hpp>
#include <Crisp/Core/StringLiteral.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {

class VulkanTracingContext {
public:
    VulkanTracingContext(const VulkanDevice& device, const VulkanPhysicalDevice& physicalDevice);
    ~VulkanTracingContext();

    VulkanTracingContext(const VulkanTracingContext&) = delete;
    VulkanTracingContext& operator=(const VulkanTracingContext&) = delete;

    VulkanTracingContext(VulkanTracingContext&&) noexcept = delete;
    VulkanTracingContext& operator=(VulkanTracingContext&&) noexcept = delete;

    void retrieveResults();

    void writeTimestamp(
        const VkCommandBuffer cmdBuffer,
        const VkPipelineStageFlags2 stageFlags,
        const LiteralWrapper name,
        const ScopeEventType eventType) {
        m_events.emplace_back(ScopeEvent{m_count, name, eventType});
        vkCmdWriteTimestamp2(cmdBuffer, stageFlags, m_queryPool, m_count++);
    }

    std::span<const ScopeEvent> getTracedEvents() const {
        return std::span(m_events).subspan(0, m_resolvedEvents);
    }

private:
    gsl::not_null<const VulkanDevice*> m_device;
    VkQueryPool m_queryPool;
    uint32_t m_maxQueryCount;
    float m_timestampPeriod;
    int64_t m_referenceTimepoint;

    uint32_t m_first;
    uint32_t m_count;

    std::vector<uint64_t> m_retrievedQueries;
    std::vector<ScopeEvent> m_events;
    uint64_t m_resolvedEvents;
};

namespace detail {
void setTraceContexts(std::span<std::unique_ptr<VulkanTracingContext>> contexts);
std::span<gsl::not_null<VulkanTracingContext*>> getTraceContexts();
VulkanTracingContext* getTraceContext();
void setTraceContextIndex(uint32_t index);
} // namespace detail

class VulkanTracingScope {
public:
    explicit VulkanTracingScope(
        VulkanTracingContext* context,
        const VkCommandBuffer cmdBuffer,
        const LiteralWrapper name,
        const VkPipelineStageFlags2 stageFlags = VK_PIPELINE_STAGE_2_NONE)
        : m_context(context)
        , m_cmdBuffer(cmdBuffer)
        , m_name(name) {
        if (context) {
            context->writeTimestamp(m_cmdBuffer, stageFlags, m_name, ScopeEventType::Begin);
        }
    }

    ~VulkanTracingScope() {
        if (m_context) {
            m_context->writeTimestamp(m_cmdBuffer, VK_PIPELINE_STAGE_2_NONE, m_name, ScopeEventType::End);
        }
    }

    VulkanTracingScope(VulkanTracingScope&&) = delete;
    VulkanTracingScope& operator=(VulkanTracingScope&&) = delete;
    VulkanTracingScope(const VulkanTracingScope&) = delete;
    VulkanTracingScope& operator=(const VulkanTracingScope&) = delete;

private:
    VulkanTracingContext* m_context;
    const VkCommandBuffer m_cmdBuffer;
    const LiteralWrapper m_name;
};

#define CRISP_TRACE_VK_SCOPE(scopeName, cmdBuffer)                                                                     \
    VulkanTracingScope CRISP_CONCATENATE(scope, __LINE__)(detail::getTraceContext(), cmdBuffer, scopeName);

#define CRISP_TRACE_VK_ADVANCE(virtualFrameIndex)                                                                      \
    {                                                                                                                  \
        detail::setTraceContextIndex(virtualFrameIndex);                                                               \
        auto* traceContext = detail::getTraceContext();                                                                \
        if (traceContext) {                                                                                            \
            traceContext->retrieveResults();                                                                           \
        }                                                                                                              \
    }

} // namespace crisp