#include <Crisp/Vulkan/Rhi/VulkanTimelineSemaphore.hpp>

#include <algorithm>
#include <limits>
#include <string>
#include <tuple>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

namespace crisp {
namespace {
// The spec guarantees maxTimelineSemaphoreValueDifference is at least 2^31 - 1.
constexpr uint64_t kMinGuaranteedValueDifference = (1ull << 31) - 1;
} // namespace

VulkanTimelineSemaphore::VulkanTimelineSemaphore(
    const VulkanDevice& device, const uint64_t initialValue, const std::string_view debugName)
    : VulkanResource(device.getResourceDeallocator())
    , m_scheduledValue(initialValue) {
    const VkSemaphoreTypeCreateInfo typeInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = initialValue,
    };
    const VkSemaphoreCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &typeInfo,
    };
    VK_FATAL(vkCreateSemaphore(device.getHandle(), &createInfo, nullptr, &m_handle));
    if (!debugName.empty()) {
        device.setObjectName(m_handle, std::string(debugName));
    }
}

uint64_t VulkanTimelineSemaphore::advance() {
    ++m_scheduledValue;
    CRISP_CHECK_LT(
        m_scheduledValue - getCompletedValue(),
        kMinGuaranteedValueDifference,
        "Timeline scheduled up to {} while the GPU has only reached {}.",
        m_scheduledValue,
        getCompletedValue());
    return m_scheduledValue;
}

uint64_t VulkanTimelineSemaphore::getCompletedValue() const {
    uint64_t value{0};
    VK_CHECK(vkGetSemaphoreCounterValue(m_deallocator->getDeviceHandle(), m_handle, &value));
    return value;
}

void VulkanTimelineSemaphore::wait(const uint64_t value) const {
    std::ignore = wait(value, std::numeric_limits<uint64_t>::max());
}

bool VulkanTimelineSemaphore::wait(const uint64_t value, const uint64_t timeoutNs) const {
    const VkSemaphoreWaitInfo waitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores = &m_handle,
        .pValues = &value,
    };
    const VkResult result = vkWaitSemaphores(m_deallocator->getDeviceHandle(), &waitInfo, timeoutNs);
    if (result == VK_TIMEOUT) {
        return false;
    }
    VK_FATAL(result);
    return true;
}

void VulkanTimelineSemaphore::signal(const uint64_t value) {
    const VkSemaphoreSignalInfo signalInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
        .semaphore = m_handle,
        .value = value,
    };
    VK_FATAL(vkSignalSemaphore(m_deallocator->getDeviceHandle(), &signalInfo));
    m_scheduledValue = std::max(m_scheduledValue, value);
}

} // namespace crisp
