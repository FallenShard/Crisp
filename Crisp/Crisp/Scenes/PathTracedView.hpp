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
#include <Crisp/Vulkan/RayTracingPipelineBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanAccelerationStructure.hpp>
#include <Crisp/Vulkan/VulkanDescriptorHeap.hpp>

namespace crisp {

// One TLAS instance: geometry in its own per-node buffers, plus the index of its record in the shared
// PbrMaterialTable. Mirrors PathTracedInstance in Shaders/PathTracer/Core/pbr-scene.part.glsl.
struct PathTracedInstance {
    VkDeviceAddress positions{0};
    VkDeviceAddress attributes{0};
    VkDeviceAddress triangles{0};
    uint32_t materialIndex{0};
    uint32_t materialTextureOffset{std::numeric_limits<uint32_t>::max()};
};

static_assert(sizeof(PathTracedInstance) == 32);
static_assert(std::is_standard_layout_v<PathTracedInstance>);
static_assert(offsetof(PathTracedInstance, positions) == 0);
static_assert(offsetof(PathTracedInstance, attributes) == 8);
static_assert(offsetof(PathTracedInstance, triangles) == 16);
static_assert(offsetof(PathTracedInstance, materialIndex) == 24);
static_assert(offsetof(PathTracedInstance, materialTextureOffset) == 28);

// Mirrors PathTracedViewAddresses in Shaders/PathTracer/Core/pbr-scene.part.glsl.
struct PathTracedViewAddresses {
    VkDeviceAddress instances{0};
    VkDeviceAddress materials{0};
};

static_assert(sizeof(PathTracedViewAddresses) == 2 * sizeof(VkDeviceAddress));

// What a scene hands over for one path-traced instance. The geometry must have been created with the shader
// device-address and acceleration-structure-input usage bits.
struct PathTracedGeometry {
    const Geometry* geometry{nullptr};
    glm::mat4 transform{1.0f};
    uint32_t materialIndex{0};
    uint32_t triangleCount{0};
    uint32_t sceneIndex{0};
    std::array<const VulkanImageView*, kPbrMapTypeCount> materialTextures{};
};

// Declares the accumulation image and the trace pass. Separate from PathTracedView because a scene has to
// compile its graph before it owns the geometry the view is built from, so the pass is registered first and the
// view is attached to it afterwards.
void addPathTracedViewPass(rg::RenderGraph& renderGraph, std::function<void(const FrameContext&)> execute);

const VulkanImageView& getPathTracedViewImage(const rg::RenderGraph& renderGraph);

// A path-traced view of a scene that is otherwise rasterized: same geometry, same PbrMaterialTable, same
// environment map. It owns its acceleration structures, its accumulation image, and the ray-tracing pipeline,
// and renders into a render-graph pass the owning scene composites.
//
// The evaluator is a deliberate stand-in for the raster material -- no textures, no direct lights, environment
// lighting by BSDF sampling only. See docs/openpbr-path-tracer.md for what replaces it.
class PathTracedView {
public:
    PathTracedView(
        Renderer& renderer,
        std::span<const PathTracedGeometry> instances,
        VkDeviceAddress materialTableAddress,
        const VulkanImageView& environmentMapView);
    ~PathTracedView();

    PathTracedView(const PathTracedView&) = delete;
    PathTracedView& operator=(const PathTracedView&) = delete;
    PathTracedView(PathTracedView&&) = delete;
    PathTracedView& operator=(PathTracedView&&) = delete;

    // Runs one accumulation step. Call from the pass registered by addPathTracedViewPass.
    void trace(const FrameContext& frameContext);

    // Re-points the heap at the graph's images. Call after every compile and resize.
    void updateDescriptorHeap(const rg::RenderGraph& renderGraph);

    void updateCamera(const CameraParameters& cameraParams);

    // Discards the accumulated estimate. Any change to the camera, the materials or the environment must call
    // this, or the new image is averaged into the old one.
    void resetAccumulation();

    void uploadFrameData(const FrameContext& frameContext);

    void drawGui(bool allowEnvironmentIntensity = true);

    int32_t getAccumulatedSampleCount() const;

    bool isEnvironmentBound() const {
        return m_environmentMapView != nullptr;
    }

    void setEnvironmentMap(const VulkanImageView& environmentMapView);

    void setSceneIndex(uint32_t sceneIndex);

    void setEnvironmentIntensity(float intensity);

    void setMaterialTextures(
        uint32_t materialIndex, const std::array<const VulkanImageView*, kPbrMapTypeCount>& textures);

private:
    std::unique_ptr<VulkanPipeline> createPipeline();

    Renderer* m_renderer;

    std::vector<std::unique_ptr<VulkanAccelerationStructure>> m_bottomLevelAccelStructures;
    std::vector<std::unique_ptr<VulkanAccelerationStructure>> m_topLevelAccelStructures;
    uint32_t m_sceneIndex{0};

    const VulkanImageView* m_environmentMapView{nullptr};

    std::unique_ptr<VulkanResourceHeap> m_resourceHeap;
    std::unique_ptr<VulkanSamplerHeap> m_samplerHeap;
    std::unique_ptr<VulkanPipeline> m_pipeline;
    ShaderBindingTable m_shaderBindingTable;

    std::unique_ptr<VulkanBuffer> m_instanceBuffer;
    std::unique_ptr<VulkanBuffer> m_cameraBuffer;
    std::unique_ptr<VulkanBuffer> m_integratorBuffer;

    PathTracedViewAddresses m_sceneAddresses;

    struct MaterialTextureBinding {
        uint32_t materialIndex;
        uint32_t heapOffset;
    };
    std::vector<MaterialTextureBinding> m_materialTextureBindings;

    struct IntegratorParameters {
        int32_t maxBounces{8};
        int32_t sampleCount{1};
        int32_t frameIdx{0};
        float environmentIntensity{1.0f};
    };

    IntegratorParameters m_integratorParams;
    CameraParameters m_cameraParams{};
};

} // namespace crisp
