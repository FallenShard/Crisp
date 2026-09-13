#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {

// Material type tags. The value doubles as the callable's index in the shader binding table, because the hit
// shader dispatches with executeCallableEXT(material.type, ...). So this list, kBsdfCallableShaders below, and
// the kBsdf* constants in Shaders/PathTracer/Core/types.part.glsl are one ordering written three times; a
// mismatch renders a silently wrong material rather than failing validation.
inline constexpr int32_t kBsdfLambertian = 0;
inline constexpr int32_t kBsdfDielectric = 1;
inline constexpr int32_t kBsdfMirror = 2;
inline constexpr int32_t kBsdfMicrofacet = 3;
inline constexpr int32_t kBsdfOrenNayar = 4;
inline constexpr int32_t kBsdfSmoothConductor = 5;
inline constexpr int32_t kBsdfRoughConductor = 6;
inline constexpr int32_t kBsdfRoughDielectric = 7;
inline constexpr int32_t kBsdfOpenPbr = 8;
inline constexpr size_t kBsdfTypeCount = 9;

// Indexed by the type tag above; the path tracer appends these to its core stages in this order.
inline constexpr std::array<std::string_view, kBsdfTypeCount> kBsdfCallableShaders{
    "PathTracer/BSDFs/lambertian.rcall",
    "PathTracer/BSDFs/dielectric.rcall",
    "PathTracer/BSDFs/mirror.rcall",
    "PathTracer/BSDFs/microfacet.rcall",
    "PathTracer/BSDFs/oren-nayar.rcall",
    "PathTracer/BSDFs/smooth-conductor.rcall",
    "PathTracer/BSDFs/rough-conductor.rcall",
    "PathTracer/BSDFs/rough-dielectric.rcall",
    "PathTracer/BSDFs/openpbr.rcall",
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
