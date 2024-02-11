#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Renderer/RendererConfig.hpp>

namespace crisp {
PipelineLayoutBuilder::PipelineLayoutBuilder(ShaderUniformInputMetadata&& metadata) // NOLINT
    : m_metadata(std::move(metadata))
    , m_createFlags(m_metadata.descriptorSetLayoutBindings.size(), 0)
    , m_setBuffered(m_metadata.descriptorSetLayoutBindings.size(), false) {}

PipelineLayoutBuilder& PipelineLayoutBuilder::defineDescriptorSet(
    const uint32_t setIndex,
    const bool isBuffered,
    std::vector<VkDescriptorSetLayoutBinding>&& bindings,
    const VkDescriptorSetLayoutCreateFlags flags) {
    if (m_metadata.descriptorSetLayoutBindings.size() <= setIndex) {
        const std::size_t newSize = static_cast<std::size_t>(setIndex) + 1;
        m_metadata.descriptorSetLayoutBindings.resize(newSize);
        m_createFlags.resize(newSize);
        m_setBuffered.resize(newSize);
    }

    m_metadata.descriptorSetLayoutBindings[setIndex] = std::move(bindings);
    m_setBuffered[setIndex] = isBuffered;
    m_createFlags[setIndex] = flags;
    return *this;
}

PipelineLayoutBuilder& PipelineLayoutBuilder::addPushConstant(
    VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size) {
    m_metadata.pushConstants.push_back({stageFlags, offset, size});
    return *this;
}

std::vector<VkDescriptorSetLayout> PipelineLayoutBuilder::createDescriptorSetLayoutHandles(VkDevice device) const {
    std::vector<VkDescriptorSetLayout> setLayouts(m_metadata.descriptorSetLayoutBindings.size(), VK_NULL_HANDLE);
    for (uint32_t i = 0; i < setLayouts.size(); i++) {
        VkDescriptorSetLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        createInfo.bindingCount = static_cast<uint32_t>(m_metadata.descriptorSetLayoutBindings[i].size());
        createInfo.pBindings = m_metadata.descriptorSetLayoutBindings[i].data();
        createInfo.flags = m_createFlags[i];

        // const VkDescriptorBindingFlags bindlessFlags =
        //     VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT |
        //     VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
        // VkDescriptorSetLayoutBindingFlagsCreateInfo bindingInfo{
        //     VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
        // bindingInfo.bindingCount = 1;
        // bindingInfo.pBindingFlags = &bindlessFlags;
        // createInfo.pNext = &bindingInfo;

        vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &setLayouts[i]);
    }

    return setLayouts;
}

std::unique_ptr<VulkanPipelineLayout> PipelineLayoutBuilder::create(
    const VulkanDevice& device, uint32_t numCopies, VkDescriptorPoolCreateFlags flags) const {
    return std::make_unique<VulkanPipelineLayout>(
        device,
        createDescriptorSetLayoutHandles(device.getHandle()),
        getDescriptorSetLayoutBindings(),
        getPushConstantRanges(),
        getDescriptorSetBufferedStatuses(),
        std::make_unique<VulkanDescriptorSetAllocator>(
            device, m_metadata.descriptorSetLayoutBindings, computeCopiesPerSet(m_setBuffered, numCopies), flags));
}

std::vector<std::vector<VkDescriptorSetLayoutBinding>> PipelineLayoutBuilder::getDescriptorSetLayoutBindings() const {
    return m_metadata.descriptorSetLayoutBindings;
}

std::vector<VkPushConstantRange> PipelineLayoutBuilder::getPushConstantRanges() const {
    return m_metadata.pushConstants;
}

std::size_t PipelineLayoutBuilder::getDescriptorSetLayoutCount() const {
    return m_metadata.descriptorSetLayoutBindings.size();
}

std::vector<bool> PipelineLayoutBuilder::getDescriptorSetBufferedStatuses() const {
    return m_setBuffered;
}

void PipelineLayoutBuilder::setDescriptorSetBuffering(int index, bool isBuffered) {
    m_setBuffered.at(index) = isBuffered;
}

void PipelineLayoutBuilder::setDescriptorDynamic(int setIndex, int bindingIndex, bool isDynamic) {
    VkDescriptorSetLayoutBinding& binding = m_metadata.descriptorSetLayoutBindings.at(setIndex).at(bindingIndex);
    if (isDynamic) {
        CRISP_CHECK(
            binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
            binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
            binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        } else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
            binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        }
    } else {
        CRISP_CHECK(
            binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
            binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);

        if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
            binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        } else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
            binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
    }
}

} // namespace crisp