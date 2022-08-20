#pragma once

#include <Crisp/Common/Result.hpp>

#include <vulkan/vulkan.h>

#define GET_DEVICE_PROC_NV(procVar) getDeviceProc(procVar, device, #procVar "NV").unwrap()
#define GET_DEVICE_PROC_KHR(procVar) getDeviceProc(procVar, device, #procVar "KHR").unwrap()

template <typename T>
inline [[nodiscard]] crisp::Result<> getDeviceProc(T& procRef, VkDevice device, const char* procName)
{
    procRef = reinterpret_cast<T>(vkGetDeviceProcAddr(device, procName));
    if (!procRef)
    {
        return crisp::resultError("Failed to get function: {}", procName);
    }

    return {};
}
