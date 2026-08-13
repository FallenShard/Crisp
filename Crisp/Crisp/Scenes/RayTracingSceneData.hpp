#pragma once

#include <cstddef>
#include <type_traits>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {

// Mirrors RayTracingSceneAddresses in Shaders/Common/path-trace-scene.part.glsl.
struct RayTracingSceneAddresses {
    VkDeviceAddress vertices{0};
    VkDeviceAddress normals{0};
    VkDeviceAddress triangles{0};
    VkDeviceAddress instances{0};
    VkDeviceAddress materials{0};
    VkDeviceAddress lights{0};
    VkDeviceAddress aliasTable{0};
};

static_assert(sizeof(RayTracingSceneAddresses) == 7 * sizeof(VkDeviceAddress));
static_assert(std::is_standard_layout_v<RayTracingSceneAddresses>);
static_assert(offsetof(RayTracingSceneAddresses, vertices) == 0);
static_assert(offsetof(RayTracingSceneAddresses, normals) == 8);
static_assert(offsetof(RayTracingSceneAddresses, triangles) == 16);
static_assert(offsetof(RayTracingSceneAddresses, instances) == 24);
static_assert(offsetof(RayTracingSceneAddresses, materials) == 32);
static_assert(offsetof(RayTracingSceneAddresses, lights) == 40);
static_assert(offsetof(RayTracingSceneAddresses, aliasTable) == 48);

} // namespace crisp
