#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Renderer/RendererConfig.hpp>

namespace crisp {
namespace {

VkDescriptorPoolCreateFlags addBindlessFlag(const std::vector<bool>& isSetBindless) {
    if (std::any_of(isSetBindless.begin(), isSetBindless.end(), [](const auto v) { return v; })) {
        return VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    }

    return 0;
}

} // namespace

PipelineLayoutBuilder::PipelineLayoutBuilder(PipelineLayoutMetadata&& metadata) // NOLINT
    : m_metadata(std::move(metadata))
    , m_createFlags(m_metadata.descriptorSetLayoutBindings.size(), 0)
    , m_setBindless(m_metadata.descriptorSetLayoutBindings.size(), false)
    , m_setBuffered(m_metadata.descriptorSetLayoutBindings.size(), false)
    , m_bindlessBindings(m_metadata.descriptorSetLayoutBindings.size()) {}

PipelineLayoutBuilder& PipelineLayoutBuilder::defineDescriptorSet(
    const uint32_t setIndex,
    const bool isBuffered,
    std::vector<VkDescriptorSetLayoutBinding>&& bindings,
    const VkDescriptorSetLayoutCreateFlags flags) {
    if (m_metadata.descriptorSetLayoutBindings.size() <= setIndex) {
        const std::size_t newSize = static_cast<std::size_t>(setIndex) + 1;
        m_metadata.descriptorSetLayoutBindings.resize(newSize);
        m_createFlags.resize(newSize);
        m_setBindless.resize(newSize);
        m_setBuffered.resize(newSize);
        m_bindlessBindings.resize(newSize);
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

        constexpr VkDescriptorBindingFlags kBindlessFlags =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
        std::vector<VkDescriptorBindingFlags> bindingFlags(createInfo.bindingCount, 0);
        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
        if (m_setBindless[i]) {
            for (const auto& bindless : m_bindlessBindings[i]) {
                bindingFlags[bindless] = kBindlessFlags;
            }
            bindingInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
            bindingInfo.pBindingFlags = bindingFlags.data();
            createInfo.pNext = &bindingInfo;
            createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        }

        vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &setLayouts[i]);
    }

    return setLayouts;
}

std::unique_ptr<VulkanPipelineLayout> PipelineLayoutBuilder::create(
    const VulkanDevice& device, const uint32_t numCopies, const VkDescriptorPoolCreateFlags flags) const {
    return std::make_unique<VulkanPipelineLayout>(
        device,
        createDescriptorSetLayoutHandles(device.getHandle()),
        getDescriptorSetLayoutBindings(),
        getPushConstantRanges(),
        getDescriptorSetBufferedStatuses(),
        getBindlessBindings(),
        std::make_unique<VulkanDescriptorSetAllocator>(
            device,
            m_metadata.descriptorSetLayoutBindings,
            computeCopiesPerSet(m_setBuffered, numCopies),
            flags | addBindlessFlag(m_setBindless)));
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

std::vector<std::vector<uint32_t>> PipelineLayoutBuilder::getBindlessBindings() const {
    return m_bindlessBindings;
}

std::vector<bool> PipelineLayoutBuilder::getDescriptorSetBufferedStatuses() const {
    return m_setBuffered;
}

void PipelineLayoutBuilder::setDescriptorSetBuffering(const uint32_t setIndex, const bool isBuffered) {
    m_setBuffered.at(setIndex) = isBuffered;
}

void PipelineLayoutBuilder::setDescriptorDynamic(
    const uint32_t setIndex, const uint32_t bindingIndex, const bool isDynamic) {
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

void PipelineLayoutBuilder::setDescriptorBindless(
    const uint32_t setIndex, const uint32_t binding, const uint32_t maxDescriptorCount) {
    m_setBindless.at(setIndex) = true;
    m_metadata.descriptorSetLayoutBindings[setIndex][binding].descriptorCount = maxDescriptorCount;
    m_bindlessBindings.at(setIndex).push_back(binding);
}

} // namespace crisp