#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {

// Material type tags. The value doubles as the callable's index in the shader binding table, because the hit
// shader dispatches with executeCallableEXT(material.type, ...). So this list, kBrdfCallableShaders below, and
// the kBrdf* constants in Shaders/PathTracer/Core/types.part.glsl are one ordering written three times; a
// mismatch renders a silently wrong material rather than failing validation.
inline constexpr int32_t kBrdfLambertian = 0;
inline constexpr int32_t kBrdfDielectric = 1;
inline constexpr int32_t kBrdfMirror = 2;
inline constexpr int32_t kBrdfMicrofacet = 3;
inline constexpr int32_t kBrdfOrenNayar = 4;
inline constexpr int32_t kBrdfSmoothConductor = 5;
inline constexpr int32_t kBrdfRoughConductor = 6;
inline constexpr int32_t kBrdfRoughDielectric = 7;
inline constexpr int32_t kBrdfOpenPbr = 8;
inline constexpr size_t kBrdfTypeCount = 9;

// Indexed by the type tag above; the path tracer appends these to its core stages in this order.
inline constexpr std::array<std::string_view, kBrdfTypeCount> kBrdfCallableShaders{
    "Brdf/path-trace-lambertian.rcall",
    "Brdf/path-trace-dielectric.rcall",
    "Brdf/path-trace-mirror.rcall",
    "Brdf/path-trace-microfacet.rcall",
    "Brdf/path-trace-oren-nayar.rcall",
    "Brdf/path-trace-smooth-conductor.rcall",
    "Brdf/path-trace-rough-conductor.rcall",
    "Brdf/path-trace-rough-dielectric.rcall",
    "Brdf/path-trace-openpbr.rcall",
};

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
