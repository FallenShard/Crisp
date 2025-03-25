#pragma once

#include <Crisp/Image/Image.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>

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

    EnvironmentLight(Renderer& renderer, const ImageBasedLightingData& iblData);

    void update(Renderer& renderer, const ImageBasedLightingData& iblData);

    void setName(const std::string& name) {
        m_name = name;
    }

    const std::string& getName() const {
        return m_name;
    }

    const VulkanImageView& getDiffuseMapView() const {
        return m_diffuseEnvironmentMap->getView();
    }

    const VulkanImageView& getSpecularMapView() const {
        return m_specularEnvironmentMap->getView();
    }

    const VulkanImageView& getCubeMapView() const {
        return m_cubeMap->getView();
    }

private:
    std::string m_name;

    std::unique_ptr<VulkanImage> m_cubeMap;
    std::unique_ptr<VulkanImage> m_diffuseEnvironmentMap;
    std::unique_ptr<VulkanImage> m_specularEnvironmentMap;
};

std::unique_ptr<VulkanImage> convertEquirectToCubeMap(Renderer* renderer, const VulkanImage& equirectMap);
std::unique_ptr<VulkanImage> integrateBrdfLut(Renderer* renderer);

} // namespace crisp
