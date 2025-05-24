#include <Crisp/Vulkan/Rhi/VulkanPipelineCache.hpp>

#include <Crisp/Core/Logger.hpp>
#include <Crisp/Io/FileUtils.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>

namespace crisp {
namespace {
CRISP_MAKE_LOGGER_ST("VulkanPipelineCache");
} // namespace

VulkanPipelineCache::VulkanPipelineCache(const VulkanDevice& device, std::filesystem::path&& cachePath)
    : VulkanResource(VK_NULL_HANDLE, device.getResourceDeallocator())
    , m_cachePath(std::move(cachePath)) {

    const auto binaryFileResult = readBinaryFile(m_cachePath);
    size_t dataSize = 0;
    const void* initialData = nullptr;
    if (binaryFileResult.hasValue()) {
        initialData = binaryFileResult->data();
        dataSize = binaryFileResult->size();
        CRISP_LOGI("Creating pipeline cache from {}.", m_cachePath.generic_string());
    }

    VkPipelineCacheCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .initialDataSize = dataSize,
        .pInitialData = initialData,
    };

    vkCreatePipelineCache(device.getHandle(), &createInfo, nullptr, &m_handle);
}

VulkanPipelineCache::~VulkanPipelineCache() {
    if (!m_handle) {
        return;
    }

    size_t size = 0;
    vkGetPipelineCacheData(m_deallocator->getDeviceHandle(), m_handle, &size, nullptr);

    std::vector<char> buffer(size);
    vkGetPipelineCacheData(m_deallocator->getDeviceHandle(), m_handle, &size, buffer.data());

    if (size > 0) {
        writeBinaryFile(m_cachePath, buffer).unwrap();
    }
}

} // namespace crisp