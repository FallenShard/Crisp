#pragma once

#include <memory>

#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>

namespace crisp {

// Must match kGgxAlbedoLutSize in Shaders/Brdf/OpenPbr/directional-albedo.part.glsl.
inline constexpr uint32_t kGgxAlbedoLutExtent = 64;

// Baked offline by tools/bake_ggx_albedo_lut.py.
std::unique_ptr<VulkanImage> loadGgxAlbedoLut(VulkanDevice& device, const std::filesystem::path& path);

} // namespace crisp
