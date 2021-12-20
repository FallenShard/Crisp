#include "PipelineLayoutBuilder.hpp"

#include <Crisp/Renderer/DescriptorSetAllocator.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/vulkan/VulkanPipelineLayout.hpp>

#include <Crisp/ShadingLanguage/Reflection.hpp>

#include <numeric>

namespace crisp
{
PipelineLayoutBuilder::PipelineLayoutBuilder(sl::ShaderUniformInputMetadata&& metadata)
    : m_setLayoutBindings(std::move(metadata.descriptorSetLayoutBindings))
    , m_setBuffered(m_setLayoutBindings.size(), false)
    , m_createFlags(m_setLayoutBindings.size(), 0)
    , m_pushConstantRanges(std::move(metadata.pushConstants))
{
}

PipelineLayoutBuilder& PipelineLayoutBuilder::defineDescriptorSet(uint32_t setIndex,
    std::vector<VkDescriptorSetLayoutBinding>&& bindings, VkDescriptorSetLayoutCreateFlags flags)
{
    return defineDescriptorSet(setIndex, false, std::move(bindings), flags);
}

PipelineLayoutBuilder& PipelineLayoutBuilder::defineDescriptorSet(uint32_t setIndex, bool isBuffered,
    std::vector<VkDescriptorSetLayoutBinding>&& bindings, VkDescriptorSetLayoutCreateFlags flags)
{
    if (m_setLayoutBindings.size() <= setIndex)
    {
        const std::size_t newSize = static_cast<std::size_t>(setIndex) + 1;
        m_setLayoutBindings.resize(newSize);
        m_createFlags.resize(newSize);
        m_setBuffered.resize(newSize);
    }

    m_setLayoutBindings[setIndex] = std::move(bindings);
    m_setBuffered[setIndex] = isBuffered;
    m_createFlags[setIndex] = flags;
    return *this;
}

PipelineLayoutBuilder& PipelineLayoutBuilder::addPushConstant(VkShaderStageFlags stageFlags, uint32_t offset,
    uint32_t size)
{
    m_pushConstantRanges.push_back({ stageFlags, offset, size });
    return *this;
}

std::vector<VkDescriptorSetLayout> PipelineLayoutBuilder::createDescriptorSetLayoutHandles(VkDevice device) const
{
    std::vector<VkDescriptorSetLayout> setLayouts(m_setLayoutBindings.size(), VK_NULL_HANDLE);
    for (uint32_t i = 0; i < setLayouts.size(); i++)
    {
        VkDescriptorSetLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        createInfo.bindingCount = static_cast<uint32_t>(m_setLayoutBindings[i].size());
        createInfo.pBindings = m_setLayoutBindings[i].data();
        createInfo.flags = m_createFlags[i];
        vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &setLayouts[i]);
    }

    return setLayouts;
}

VkPipelineLayout PipelineLayoutBuilder::createHandle(VkDevice device, VkDescriptorSetLayout* setLayouts,
    uint32_t setLayoutCount)
{
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount = setLayoutCount;
    pipelineLayoutInfo.pSetLayouts = setLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(m_pushConstantRanges.size());
    pipelineLayoutInfo.pPushConstantRanges = m_pushConstantRanges.data();

    VkPipelineLayout layout;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout);
    return layout;
}

std::unique_ptr<VulkanPipelineLayout> PipelineLayoutBuilder::create(VulkanDevice& device, uint32_t numCopies,
    VkDescriptorPoolCreateFlags flags) const
{
    return std::make_unique<VulkanPipelineLayout>(device, createDescriptorSetLayoutHandles(device.getHandle()),
        getDescriptorSetLayoutBindings(), getPushConstantRanges(), getDescriptorSetBufferedStatuses(),
        createMinimalDescriptorSetAllocator(device, numCopies, flags));
}

std::unique_ptr<DescriptorSetAllocator> PipelineLayoutBuilder::createMinimalDescriptorSetAllocator(VulkanDevice& device,
    uint32_t numCopies, VkDescriptorPoolCreateFlags flags) const
{
    return std::make_unique<DescriptorSetAllocator>(device, m_setLayoutBindings, getNumCopiesPerSet(numCopies), flags);
}

std::vector<std::vector<VkDescriptorSetLayoutBinding>> PipelineLayoutBuilder::getDescriptorSetLayoutBindings() const
{
    return m_setLayoutBindings;
}

std::vector<VkPushConstantRange> PipelineLayoutBuilder::getPushConstantRanges() const
{
    return m_pushConstantRanges;
}

std::size_t PipelineLayoutBuilder::getDescriptorSetLayoutCount() const
{
    return m_setLayoutBindings.size();
}

std::vector<bool> PipelineLayoutBuilder::getDescriptorSetBufferedStatuses() const
{
    return m_setBuffered;
}

void PipelineLayoutBuilder::setDescriptorSetBuffering(int index, bool isBuffered)
{
    m_setBuffered.at(index) = isBuffered;
}

void PipelineLayoutBuilder::setDescriptorDynamic(int setIndex, int bindingIndex, bool isDynamic)
{
    VkDescriptorSetLayoutBinding& binding = m_setLayoutBindings.at(setIndex).at(bindingIndex);
    if (isDynamic)
    {
        if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    }
    else
    {
        if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
            binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
            binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
}

std::vector<uint32_t> PipelineLayoutBuilder::getNumCopiesPerSet(uint32_t numCopies) const
{
    std::vector<uint32_t> numCopiesPerSet;
    for (uint32_t i = 0; i < m_setBuffered.size(); ++i)
        numCopiesPerSet.push_back(m_setBuffered[i] ? numCopies * RendererConfig::VirtualFrameCount : numCopies);
    return numCopiesPerSet;
}
} // namespace crisp