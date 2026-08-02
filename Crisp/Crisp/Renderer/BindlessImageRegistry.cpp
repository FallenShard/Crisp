#include <Crisp/Renderer/BindlessImageRegistry.hpp>

#include <algorithm>
#include <array>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

namespace crisp {
namespace {

constexpr VkDescriptorBindingFlags kBindlessBindingFlags =
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

uint32_t clampToLimit(const uint32_t requested, const uint32_t perSetLimit, const uint32_t perStageLimit) {
    return std::min({requested, perSetLimit, perStageLimit});
}

BindlessImageRegistryConfig clampToDeviceLimits(
    const BindlessImageRegistryConfig& config, const VulkanPhysicalDevice& physicalDevice) {
    const auto& limits = physicalDevice.getCapabilities().properties12;
    return {
        .sampledImageCapacity = clampToLimit(
            config.sampledImageCapacity,
            limits.maxDescriptorSetUpdateAfterBindSampledImages,
            limits.maxPerStageDescriptorUpdateAfterBindSampledImages),
        .storageImageCapacity = clampToLimit(
            config.storageImageCapacity,
            limits.maxDescriptorSetUpdateAfterBindStorageImages,
            limits.maxPerStageDescriptorUpdateAfterBindStorageImages),
        .samplerCapacity = clampToLimit(
            config.samplerCapacity,
            limits.maxDescriptorSetUpdateAfterBindSamplers,
            limits.maxPerStageDescriptorUpdateAfterBindSamplers),
    };
}

} // namespace

void BindlessImageRegistry::SlotAllocator::initialize(const uint32_t slotCapacity) {
    capacity = slotCapacity;
    generations.assign(slotCapacity, 0);
    debugInfo.resize(slotCapacity);

    // Descending, so the stack hands out ascending indices - RenderDoc and the ImGui inspector read better.
    freeList.reserve(slotCapacity - 1);
    for (uint32_t i = slotCapacity; i > kDefaultSlot + 1; --i) {
        freeList.push_back(i - 1);
    }
}

uint32_t BindlessImageRegistry::SlotAllocator::allocate() {
    if (freeList.empty()) {
        CRISP_FATAL("Bindless image array of capacity {} is exhausted.", capacity);
    }

    const uint32_t index = freeList.back();
    freeList.pop_back();
    return index;
}

BindlessImageRegistry::BindlessImageRegistry(
    const VulkanDevice& device, const VulkanPhysicalDevice& physicalDevice, const BindlessImageRegistryConfig& config)
    : m_device(&device) {
    const auto clampedConfig = clampToDeviceLimits(config, physicalDevice);

    m_sampledImages.initialize(clampedConfig.sampledImageCapacity);
    m_storageImages.initialize(clampedConfig.storageImageCapacity);
    m_samplerCapacity = clampedConfig.samplerCapacity;
    m_samplerDebugInfo.resize(m_samplerCapacity);

    createSetLayout(clampedConfig);
    createPoolAndSet(clampedConfig);

    device.setObjectName(m_setLayout, "BindlessImageRegistry-SetLayout");
    device.setObjectName(m_pool, "BindlessImageRegistry-Pool");
    device.setObjectName(m_set, "BindlessImageRegistry-Set");
}

BindlessImageRegistry::~BindlessImageRegistry() {
    vkDestroyDescriptorPool(m_device->getHandle(), m_pool, nullptr);
    vkDestroyDescriptorSetLayout(m_device->getHandle(), m_setLayout, nullptr);
}

void BindlessImageRegistry::createSetLayout(const BindlessImageRegistryConfig& config) {
    const std::array<VkDescriptorSetLayoutBinding, 3> bindings{{
        {kSampledImageBinding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, config.sampledImageCapacity, VK_SHADER_STAGE_ALL, nullptr},
        {kStorageImageBinding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, config.storageImageCapacity, VK_SHADER_STAGE_ALL, nullptr},
        {kSamplerBinding, VK_DESCRIPTOR_TYPE_SAMPLER, config.samplerCapacity, VK_SHADER_STAGE_ALL, nullptr},
    }};
    const std::array<VkDescriptorBindingFlags, bindings.size()> bindingFlags{
        kBindlessBindingFlags, kBindlessBindingFlags, kBindlessBindingFlags};

    // No VARIABLE_DESCRIPTOR_COUNT: the arrays are allocated at their declared capacity, so there is nothing
    // for a variable count to shrink.
    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    flagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
    flagsInfo.pBindingFlags = bindingFlags.data();

    VkDescriptorSetLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    createInfo.pNext = &flagsInfo;
    createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    createInfo.pBindings = bindings.data();

    VK_FATAL(vkCreateDescriptorSetLayout(m_device->getHandle(), &createInfo, nullptr, &m_setLayout));
}

void BindlessImageRegistry::createPoolAndSet(const BindlessImageRegistryConfig& config) {
    const std::array<VkDescriptorPoolSize, 3> poolSizes{{
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, config.sampledImageCapacity},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, config.storageImageCapacity},
        {VK_DESCRIPTOR_TYPE_SAMPLER, config.samplerCapacity},
    }};

    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    VK_FATAL(vkCreateDescriptorPool(m_device->getHandle(), &poolInfo, nullptr, &m_pool));

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = m_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_setLayout;
    VK_FATAL(vkAllocateDescriptorSets(m_device->getHandle(), &allocInfo, &m_set));
}

void BindlessImageRegistry::queueWrite(
    const uint32_t binding, const uint32_t arrayIndex, const VkDescriptorType type, const VkDescriptorImageInfo& info) {
    m_pendingWrites.push_back({binding, arrayIndex, type, info});
}

void BindlessImageRegistry::setDefaultSampledImage(const VulkanImageView& view) {
    m_defaultSampledImage = {
        .sampler = VK_NULL_HANDLE,
        .imageView = view.getHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    for (uint32_t i = 0; i < m_sampledImages.capacity; ++i) {
        queueWrite(kSampledImageBinding, i, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_defaultSampledImage);
    }
    m_sampledImages.debugInfo[kDefaultSlot] = {.name = "<default sampled image>", .state = BindlessSlotState::Live};
}

void BindlessImageRegistry::setDefaultStorageImage(const VulkanImageView& view) {
    m_defaultStorageImage = {
        .sampler = VK_NULL_HANDLE,
        .imageView = view.getHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    for (uint32_t i = 0; i < m_storageImages.capacity; ++i) {
        queueWrite(kStorageImageBinding, i, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_defaultStorageImage);
    }
    m_storageImages.debugInfo[kDefaultSlot] = {.name = "<default storage image>", .state = BindlessSlotState::Live};
}

uint32_t BindlessImageRegistry::acquireSlot(SlotAllocator& allocator, const std::string_view name) {
    const uint32_t index = allocator.allocate();
    allocator.debugInfo[index] = {
        .name = std::string(name),
        .registeredAt = m_retirementValue,
        .releasedAt = 0,
        .state = BindlessSlotState::Live,
    };
    return index;
}

SampledImageHandle BindlessImageRegistry::addSampledImage(
    const VulkanImageView& view, const std::string_view name, const VkImageLayout layout) {
    const uint32_t index = acquireSlot(m_sampledImages, name);
    queueWrite(
        kSampledImageBinding,
        index,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        {.sampler = VK_NULL_HANDLE, .imageView = view.getHandle(), .imageLayout = layout});
    return {index, m_sampledImages.generations[index]};
}

StorageImageHandle BindlessImageRegistry::addStorageImage(const VulkanImageView& view, const std::string_view name) {
    const uint32_t index = acquireSlot(m_storageImages, name);
    queueWrite(
        kStorageImageBinding,
        index,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        {.sampler = VK_NULL_HANDLE, .imageView = view.getHandle(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL});
    return {index, m_storageImages.generations[index]};
}

void BindlessImageRegistry::releaseSlot(
    SlotAllocator& allocator, const uint32_t binding, const uint32_t index, const uint32_t generation) {
    CRISP_CHECK_LT(index, allocator.capacity);
    CRISP_CHECK_NE(index, kDefaultSlot, "The default bindless slot cannot be released.");
    CRISP_CHECK_EQ(generation, allocator.generations[index], "Releasing a bindless handle to an already recycled slot.");

    // Bumped now rather than on recycle, so any surviving copy of the handle fails validation immediately.
    ++allocator.generations[index];
    allocator.debugInfo[index].releasedAt = m_retirementValue;
    allocator.debugInfo[index].state = BindlessSlotState::Retiring;
    m_pendingFrees.push_back({m_retirementValue, binding, index});
}

void BindlessImageRegistry::remove(const SampledImageHandle handle) {
    releaseSlot(m_sampledImages, kSampledImageBinding, handle.index(), handle.generation());
}

void BindlessImageRegistry::remove(const StorageImageHandle handle) {
    releaseSlot(m_storageImages, kStorageImageBinding, handle.index(), handle.generation());
}

uint32_t BindlessImageRegistry::addSampler(const VulkanSampler& sampler, const std::string_view name) {
    if (m_samplerCount >= m_samplerCapacity) {
        CRISP_FATAL("Bindless sampler array of capacity {} is exhausted.", m_samplerCapacity);
    }

    const uint32_t index = m_samplerCount++;
    m_samplerDebugInfo[index] = {
        .name = std::string(name),
        .registeredAt = m_retirementValue,
        .state = BindlessSlotState::Live,
    };
    queueWrite(
        kSamplerBinding,
        index,
        VK_DESCRIPTOR_TYPE_SAMPLER,
        {.sampler = sampler.getHandle(), .imageView = VK_NULL_HANDLE, .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED});
    return index;
}

void BindlessImageRegistry::setRetirementValue(const uint64_t value) {
    m_retirementValue = value;
}

void BindlessImageRegistry::collect(const uint64_t completedValue) {
    const auto matured = std::ranges::partition(m_pendingFrees, [completedValue](const PendingFree& pendingFree) {
        return pendingFree.retirementValue > completedValue;
    });

    for (const auto& pendingFree : matured) {
        // Restoring the default is deferred to here, not done in remove(): overwriting a descriptor that
        // in-flight work may still read is what UPDATE_AFTER_BIND does not permit. A slot reallocated before
        // the next flush queues its real write afterwards, and vkUpdateDescriptorSets applies writes in order.
        auto& allocator = pendingFree.binding == kSampledImageBinding ? m_sampledImages : m_storageImages;
        if (pendingFree.binding == kSampledImageBinding) {
            queueWrite(kSampledImageBinding, pendingFree.index, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_defaultSampledImage);
        } else {
            queueWrite(kStorageImageBinding, pendingFree.index, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_defaultStorageImage);
        }

        // The name is left in place - it is what the slot last held, which is the useful thing to report when a
        // stale index turns up.
        allocator.debugInfo[pendingFree.index].state = BindlessSlotState::Free;
        allocator.freeList.push_back(pendingFree.index);
    }

    m_pendingFrees.erase(matured.begin(), matured.end());
}

void BindlessImageRegistry::flush() {
    if (m_pendingWrites.empty()) {
        return;
    }

    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(m_pendingWrites.size());
    for (const auto& pendingWrite : m_pendingWrites) {
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = m_set;
        write.dstBinding = pendingWrite.binding;
        write.dstArrayElement = pendingWrite.arrayIndex;
        write.descriptorCount = 1;
        write.descriptorType = pendingWrite.type;
        write.pImageInfo = &pendingWrite.imageInfo;
        writes.push_back(write);
    }

    vkUpdateDescriptorSets(m_device->getHandle(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    m_pendingWrites.clear();
}

void BindlessImageRegistry::bind(
    const VkCommandBuffer cmdBuffer, const VkPipelineLayout pipelineLayout, const VkPipelineBindPoint bindPoint) const {
    vkCmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayout, 0, 1, &m_set, 0, nullptr);
}

} // namespace crisp
