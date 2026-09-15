#pragma once

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <vector>

#include <Crisp/Camera/Camera.hpp>
#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Materials/PbrMaterial.hpp>
#include <Crisp/Math/Headers.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Scenes/PathTracer.hpp>

namespace crisp {

struct PathTracedGeometry {
    const Geometry* geometry{nullptr};
    glm::mat4 transform{1.0f};
    uint32_t materialIndex{0};
    uint32_t triangleCount{0};
    uint32_t sceneIndex{0};
    uint8_t visibilityMask{0xFF};
    std::array<const VulkanImageView*, kPbrMapTypeCount> materialTextures{};
};

enum class EnergyCompensation : uint32_t { // NOLINT
    None = 0,
    KullaConty = 1,
    Turquin = 2,
};

void addPathTracedViewPass(rg::RenderGraph& renderGraph, std::function<void(const FrameContext&)> execute);

const VulkanImageView& getPathTracedViewImage(const rg::RenderGraph& renderGraph);

class PathTracedView {
public:
    PathTracedView(
        Renderer& renderer, std::span<const PathTracedGeometry> instances, VkDeviceAddress materialTableAddress);
    ~PathTracedView();

    PathTracedView(const PathTracedView&) = delete;
    PathTracedView& operator=(const PathTracedView&) = delete;
    PathTracedView(PathTracedView&&) = delete;
    PathTracedView& operator=(PathTracedView&&) = delete;

    void trace(const FrameContext& frameContext);
    void updateDescriptorHeap(const rg::RenderGraph& renderGraph);

    void updateCamera(const CameraParameters& cameraParams);

    void resetAccumulation();

    void uploadFrameData(const FrameContext& frameContext);

    void drawGui(bool allowEnvironmentIntensity = true);

    int32_t getAccumulatedSampleCount() const;

    // Next-event estimation samples the equirectangular map through its own CDF, so the miss shader has to read
    // the same image: MIS weights are only valid when both strategies see one environment function.
    void setEnvironmentDistribution(
        const VulkanImageView& equirectView, std::span<const float> cdf, uint32_t width, uint32_t height);

    void setSceneIndex(uint32_t sceneIndex);

    void setVisibilityMask(uint8_t mask);

    void setEnvironmentIntensity(float intensity);

    void setEnergyCompensation(EnergyCompensation mode);

    float getEnvironmentIntensity() const {
        return m_integratorParams.environmentIntensity;
    }

    EnergyCompensation getEnergyCompensation() const {
        return static_cast<EnergyCompensation>(m_sceneAddresses.energyCompensation);
    }

    void setMaterialTextures(
        uint32_t materialIndex, const std::array<const VulkanImageView*, kPbrMapTypeCount>& textures);

private:
    Renderer* m_renderer;
    std::unique_ptr<PathTracer> m_pathTracer;

    std::unique_ptr<VulkanBuffer> m_instanceBuffer;

    std::unique_ptr<VulkanBuffer> m_environmentCdfBuffer;
    const VulkanImageView* m_environmentView{nullptr};

    std::unique_ptr<VulkanImage> m_ggxAlbedoLut;

    PathTracedSceneAddresses m_sceneAddresses;

    struct MaterialTextureBinding {
        uint32_t materialIndex;
        uint32_t heapOffset;
    };

    std::vector<MaterialTextureBinding> m_materialTextureBindings;

    // Must match the IntegratorParams block in Shaders/PathTracer/pbr-trace.rgen.glsl.
    struct IntegratorParameters {
        int32_t maxBounces{8};
        int32_t sampleCount{1};
        int32_t frameIdx{0};
        float environmentIntensity{1.0f};
        uint32_t visibilityMask{0xFF};
        int32_t environmentWidth{0};
        int32_t environmentHeight{0};
    };

    static_assert(sizeof(IntegratorParameters) == 28);
    static_assert(offsetof(IntegratorParameters, visibilityMask) == 16);
    static_assert(offsetof(IntegratorParameters, environmentWidth) == 20);
    static_assert(offsetof(IntegratorParameters, environmentHeight) == 24);

    IntegratorParameters m_integratorParams;
};

} // namespace crisp
