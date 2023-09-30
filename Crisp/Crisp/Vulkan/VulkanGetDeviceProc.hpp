#pragma once

#include <Crisp/Core/Result.hpp>

#include <Crisp/Vulkan/VulkanHeader.hpp>

#define GET_DEVICE_PROC_NV(procVar) getDeviceProc(procVar, device, #procVar "NV").unwrap()
#define GET_DEVICE_PROC_KHR(procVar) getDeviceProc(procVar, device, #procVar "KHR").unwrap()

namespace crisp {
template <typename T>
inline Result<> getDeviceProc(T& procRef, VkDevice device, const char* procName) {
    procRef = reinterpret_cast<T>(vkGetDeviceProcAddr(device, procName)); // NOLINT
    if (!procRef) {
        return resultError("Failed to get function: {}", procName);
    }

    return {};
}
} // namespace crisp
