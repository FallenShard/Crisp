#pragma once

#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>

namespace crisp
{
class Renderer;

inline constexpr uint32_t DiffuseEnvironmentCubeMapSize = 64;
inline constexpr uint32_t SpecularEnvironmentCubeMapSize = 512;

std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> convertEquirectToCubeMap(
    Renderer* renderer, std::shared_ptr<VulkanImageView> equirectMapView, uint32_t cubeMapSize);
std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> setupDiffuseEnvMap(
    Renderer* renderer, const VulkanImageView& cubeMapView, uint32_t cubeMapSize = DiffuseEnvironmentCubeMapSize);
std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> setupReflectEnvMap(
    Renderer* renderer, const VulkanImageView& cubeMapView, uint32_t cubeMapSize = SpecularEnvironmentCubeMapSize);
std::unique_ptr<VulkanImage> integrateBrdfLut(Renderer* renderer);

} // namespace crisp
