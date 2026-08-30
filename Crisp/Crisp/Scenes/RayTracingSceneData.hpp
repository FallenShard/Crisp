#pragma once

#include <cstddef>
#include <type_traits>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {

// Mirrors RayTracingSceneAddresses in Shaders/Common/path-trace-scene.part.glsl.
struct RayTracingSceneAddresses {
    VkDeviceAddress vertices{0};
    VkDeviceAddress normals{0};
    VkDeviceAddress texCoords{0};
    VkDeviceAddress triangles{0};
    VkDeviceAddress instances{0};
    VkDeviceAddress materials{0};
    VkDeviceAddress lights{0};
    VkDeviceAddress aliasTable{0};
    VkDeviceAddress environmentCdf{0};
};

static_assert(sizeof(RayTracingSceneAddresses) == 9 * sizeof(VkDeviceAddress));
static_assert(std::is_standard_layout_v<RayTracingSceneAddresses>);
static_assert(offsetof(RayTracingSceneAddresses, vertices) == 0);
static_assert(offsetof(RayTracingSceneAddresses, normals) == 8);
static_assert(offsetof(RayTracingSceneAddresses, texCoords) == 16);
static_assert(offsetof(RayTracingSceneAddresses, triangles) == 24);
static_assert(offsetof(RayTracingSceneAddresses, instances) == 32);
static_assert(offsetof(RayTracingSceneAddresses, materials) == 40);
static_assert(offsetof(RayTracingSceneAddresses, lights) == 48);
static_assert(offsetof(RayTracingSceneAddresses, aliasTable) == 56);
static_assert(offsetof(RayTracingSceneAddresses, environmentCdf) == 64);

} // namespace crisp
