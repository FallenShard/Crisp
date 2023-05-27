#pragma once

#include <filesystem>

#include <Crisp/Image/Image.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>

namespace crisp
{
class Renderer;

inline constexpr uint32_t kDiffuseEnvironmentCubeMapSize = 64;
inline constexpr uint32_t kSpecularEnvironmentCubeMapSize = 512;

struct ImageBasedLightingData
{
    Image equirectangularEnvironmentMap;
    std::vector<Image> diffuseIrradianceCubeMap;
    std::vector<std::vector<Image>> specularReflectanceMapMipLevels;
};

ImageBasedLightingData loadImageBasedLightingData(const std::filesystem::path& path);

std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> convertEquirectToCubeMap(
    Renderer* renderer, std::shared_ptr<VulkanImageView> equirectMapView, uint32_t cubeMapSize);
std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> setupDiffuseEnvMap(
    Renderer* renderer, const VulkanImageView& cubeMapView, uint32_t cubeMapSize = kDiffuseEnvironmentCubeMapSize);
std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> setupReflectEnvMap(
    Renderer* renderer, const VulkanImageView& cubeMapView, uint32_t cubeMapSize = kSpecularEnvironmentCubeMapSize);
std::unique_ptr<VulkanImage> integrateBrdfLut(Renderer* renderer);

} // namespace crisp
