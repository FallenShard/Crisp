#pragma once

#include <Crisp/Image/Image.hpp>
#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>

namespace crisp {
inline constexpr uint32_t kCubeMapFaceCount = 6;

struct ImageBasedLightingData {
    Image equirectangularEnvironmentMap;
    std::vector<Image> diffuseIrradianceCubeMap;
    std::vector<std::vector<Image>> specularReflectanceMapMipLevels;
};

class EnvironmentLight {
public:
    static constexpr uint32_t kDiffuseCubeMapSize = 64;
    static constexpr uint32_t kSpecularCubeMapSize = 512;

    EnvironmentLight(Renderer& renderer, ImageBasedLightingData&& iblData);

    void update(Renderer& renderer, ImageBasedLightingData&& iblData);

    void setName(const std::string& name) {
        m_name = name;
    }

    const std::string& getName() const {
        return m_name;
    }

    inline const VulkanImageView& getDiffuseMapView() const {
        return *m_diffuseEnvironmentMapView;
    }

    inline const VulkanImageView& getSpecularMapView() const {
        return *m_specularEnvironmentMapView;
    }

    inline std::unique_ptr<Skybox> createSkybox(
        Renderer& renderer, const VulkanRenderPass& renderPass, const VulkanSampler& sampler) const {
        return std::make_unique<Skybox>(&renderer, renderPass, *m_cubeMapView, sampler);
    }

private:
    std::string m_name;

    std::unique_ptr<VulkanImage> m_cubeMap;
    std::unique_ptr<VulkanImageView> m_cubeMapView;

    std::unique_ptr<VulkanImage> m_diffuseEnvironmentMap;
    std::unique_ptr<VulkanImageView> m_diffuseEnvironmentMapView;

    std::unique_ptr<VulkanImage> m_specularEnvironmentMap;
    std::unique_ptr<VulkanImageView> m_specularEnvironmentMapView;
};

std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> convertEquirectToCubeMap(
    Renderer* renderer, std::shared_ptr<VulkanImageView> equirectMapView, uint32_t cubeMapSize);
std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> setupDiffuseEnvMap(
    Renderer* renderer,
    const VulkanImageView& cubeMapView,
    uint32_t cubeMapSize = EnvironmentLight::kDiffuseCubeMapSize);
std::pair<std::unique_ptr<VulkanImage>, std::unique_ptr<VulkanImageView>> setupReflectEnvMap(
    Renderer* renderer,
    const VulkanImageView& cubeMapView,
    uint32_t cubeMapSize = EnvironmentLight::kSpecularCubeMapSize);
std::unique_ptr<VulkanImage> integrateBrdfLut(Renderer* renderer);

} // namespace crisp
