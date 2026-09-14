#pragma once

#include <cstddef>
#include <type_traits>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {

// Mirrors RayTracingSceneAddresses in Shaders/PathTracer/Core/scene.part.glsl.
struct RayTracingSceneAddresses {
    VkDeviceAddress instances{0};
    VkDeviceAddress materials{0};
    VkDeviceAddress lights{0};
    VkDeviceAddress environmentCdf{0};
};

static_assert(sizeof(RayTracingSceneAddresses) == 4 * sizeof(VkDeviceAddress));
static_assert(std::is_standard_layout_v<RayTracingSceneAddresses>);
static_assert(offsetof(RayTracingSceneAddresses, instances) == 0);
static_assert(offsetof(RayTracingSceneAddresses, materials) == 8);
static_assert(offsetof(RayTracingSceneAddresses, lights) == 16);
static_assert(offsetof(RayTracingSceneAddresses, environmentCdf) == 24);

} // namespace crisp
