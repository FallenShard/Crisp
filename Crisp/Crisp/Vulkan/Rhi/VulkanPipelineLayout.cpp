#include <Crisp/Vulkan/Rhi/VulkanPipelineLayout.hpp>

#include <ranges>

namespace crisp {
namespace {
VkPipelineLayout createHandle(
    VkDevice device,
    const std::vector<VkDescriptorSetLayout>& setLayouts,
    const std::vector<VkPushConstantRange>& pushConstants) {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
    pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();

    VkPipelineLayout layout{VK_NULL_HANDLE};
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout);
    return layout;
}
} // namespace

VulkanPipelineLayout::VulkanPipelineLayout(
    const VulkanDevice& device,
    std::vector<VkDescriptorSetLayout>&& setLayouts,
    std::vector<std::vector<VkDescriptorSetLayoutBinding>>&& setBindings,
    std::vector<VkPushConstantRange>&& pushConstants,
    std::vector<bool> descriptorSetBufferedStatus,
    std::vector<std::vector<uint32_t>> bindlessIndices,
    std::unique_ptr<VulkanDescriptorSetAllocator> setAllocator)
    : VulkanResource(createHandle(device.getHandle(), setLayouts, pushConstants), device.getResourceDeallocator())
    , m_descriptorSetLayouts(setLayouts.size())
    , m_pushConstants(std::move(pushConstants))
    , m_dynamicBufferCount(0)
    , m_setAllocator(std::move(setAllocator)) {
    for (std::size_t s = 0; s < m_descriptorSetLayouts.size(); ++s) {
        const size_t bindingCount = setBindings[s].size();
        m_descriptorSetLayouts[s].handle = setLayouts[s];
        m_descriptorSetLayouts[s].bindings = std::move(setBindings[s]);
        m_descriptorSetLayouts[s].dynamicBufferIndices.resize(bindingCount, ~0u);
        m_descriptorSetLayouts[s].isBuffered = descriptorSetBufferedStatus[s];
        m_descriptorSetLayouts[s].bindlessIndices = bindlessIndices[s];

        for (std::size_t b = 0; b < m_descriptorSetLayouts[s].bindings.size(); ++b) {
            const auto& binding = m_descriptorSetLayouts[s].bindings[b];
            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
                m_descriptorSetLayouts[s].dynamicBufferIndices[b] = m_dynamicBufferCount++;
                m_descriptorSetLayouts[s].dynamicBufferCount++;
            }
        }
    }
}

VulkanPipelineLayout::~VulkanPipelineLayout() {
    for (const auto& setLayout : m_descriptorSetLayouts) {
        m_deallocator->deferDestruction(
            m_framesToLive, setLayout.handle, [](void* handle, VulkanResourceDeallocator* deallocator) {
                spdlog::debug("Destroying set layout: {}", handle);
                vkDestroyDescriptorSetLayout(
                    deallocator->getDeviceHandle(), static_cast<VkDescriptorSetLayout>(handle), nullptr);
            });
    }
}

VkDescriptorSet VulkanPipelineLayout::allocateSet(uint32_t setIndex) const {
    return m_setAllocator->allocate(
        m_descriptorSetLayouts.at(setIndex).handle,
        m_descriptorSetLayouts.at(setIndex).bindings,
        m_descriptorSetLayouts.at(setIndex).bindlessIndices);
}

void VulkanPipelineLayout::swap(VulkanPipelineLayout& other) noexcept {
    std::swap(m_descriptorSetLayouts, other.m_descriptorSetLayouts);
    std::swap(m_pushConstants, other.m_pushConstants);
    std::swap(m_dynamicBufferCount, other.m_dynamicBufferCount);
}

std::unique_ptr<VulkanDescriptorSetAllocator> VulkanPipelineLayout::createVulkanDescriptorSetAllocator(
    VulkanDevice& device, uint32_t numCopies, VkDescriptorPoolCreateFlags flags) {
    const auto isSetBuffered =
        std::ranges::transform_view(m_descriptorSetLayouts, [](const auto& layout) { return layout.isBuffered; }) |
        std::ranges::to<std::vector<bool>>();

    std::vector<std::vector<VkDescriptorSetLayoutBinding>> bindings(m_descriptorSetLayouts.size());
    for (uint32_t i = 0; i < bindings.size(); ++i) {
        bindings[i] = m_descriptorSetLayouts[i].bindings;
    }

    return std::make_unique<VulkanDescriptorSetAllocator>(
        device, bindings, computeCopiesPerSet(isSetBuffered, numCopies), flags);
}

std::vector<uint32_t> computeCopiesPerSet(const std::vector<bool>& isSetBuffered, uint32_t numCopies) {
    std::vector<uint32_t> numCopiesPerSet;
    numCopiesPerSet.reserve(isSetBuffered.size());
    for (const bool setBuffered : isSetBuffered) {
        numCopiesPerSet.push_back(setBuffered ? numCopies * kRendererVirtualFrameCount : numCopies);
    }
    return numCopiesPerSet;
}

} // namespace crisp
