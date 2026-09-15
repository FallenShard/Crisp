#pragma once

#include <array>

#include <Crisp/Image/Image.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanRingBuffer.hpp>

namespace crisp {
inline constexpr uint32_t kCubeMapFaceCount = 6;
inline constexpr uint32_t kDiffuseIrradianceShCoefficientCount = 9;
inline constexpr uint32_t kDiffuseIrradianceShChannelCount = 3;

using DiffuseIrradianceSh =
    std::array<float, kDiffuseIrradianceShCoefficientCount * kDiffuseIrradianceShChannelCount>;

struct ImageBasedLightingData {
    Image equirectangularEnvironmentMap;
    DiffuseIrradianceSh diffuseIrradianceSh{};
    std::vector<std::vector<Image>> specularReflectanceMapMipLevels;
};

class EnvironmentLight {
public:
    static constexpr uint32_t kSpecularCubeMapSize = 512;

    EnvironmentLight(Renderer& renderer, const ImageBasedLightingData& iblData);

    void update(Renderer& renderer, const ImageBasedLightingData& iblData);

    void setName(const std::string& name) {
        m_name = name;
    }

    const std::string& getName() const {
        return m_name;
    }

    const VulkanRingBuffer& getDiffuseIrradianceShBuffer() const {
        return *m_diffuseIrradianceShBuffer;
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
    std::unique_ptr<VulkanRingBuffer> m_diffuseIrradianceShBuffer;
    std::unique_ptr<VulkanImage> m_specularEnvironmentMap;
};

std::unique_ptr<VulkanImage> convertEquirectToCubeMap(Renderer* renderer, const VulkanImage& equirectMap);

inline constexpr const char* kWhiteFurnaceEnvironmentName{"(White Furnace)"};

ImageBasedLightingData createWhiteFurnaceIblData();
Image createWhiteFurnaceEquirect();

// Must match LUT_SIZE in tools/bake_brdf_lut.py.
inline constexpr uint32_t kBrdfLutExtent = 512;

// Baked offline by tools/bake_brdf_lut.py.
std::unique_ptr<VulkanImage> loadBrdfLut(Renderer* renderer);

} // namespace crisp
