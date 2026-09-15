#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <Crisp/Camera/Camera.hpp>
#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Math/Headers.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/RayTracingPipelineBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanAccelerationStructure.hpp>
#include <Crisp/Vulkan/VulkanDescriptorHeap.hpp>

namespace crisp {

// The descriptor heap layout shared by every path tracer. Must match
// Shaders/PathTracer/Core/heap-slots.part.glsl; nothing in the build checks the two against each other.
//
// Slots 0-3 are written by PathTracer itself, the rest by the tracer built on top of it. Both halves live
// here because the two tracers previously agreed on these numbers only by coincidence.
inline constexpr uint32_t kPathTracerBvhSlot = 0;
inline constexpr uint32_t kPathTracerImageSlot = 1;
inline constexpr uint32_t kPathTracerViewSlot = 2;
inline constexpr uint32_t kPathTracerIntegratorSlot = 3;
inline constexpr uint32_t kPathTracerFirstFreeSlot = 4;

// The equirectangular environment map. The miss shader and next-event estimation must read the same image, or
// the MIS weights combine two different functions.
inline constexpr uint32_t kPathTracerEnvironmentSlot = 4;
inline constexpr uint32_t kPathTracerGgxAlbedoLutSlot = 5;
// Material textures run from here to the end of the heap.
inline constexpr uint32_t kPathTracerMaterialTextureFirstSlot = 6;

inline constexpr uint32_t kPathTracerEnvironmentSamplerSlot = 0;
inline constexpr uint32_t kPathTracerMaterialSamplerSlot = 1;
inline constexpr uint32_t kPathTracerGgxAlbedoLutSamplerSlot = 2;
inline constexpr uint32_t kPathTracerSamplerHeapSlotCount = 3;

inline constexpr uint32_t kInvalidMaterialTextureOffset = 0xFFFFFFFFu;

// Mirrors the IntegratorParams block in Shaders/PathTracer/Core/integrator.part.glsl.
struct PathTracedIntegratorParams {
    int32_t maxBounces{8};
    int32_t sampleCount{1};
    int32_t frameIdx{0};
    int32_t sampleOffset{0};

    uint32_t seed{0};
    int32_t reconstructionFilter{0};
    int32_t lightCount{0};
    int32_t shapeCount{0};

    int32_t samplingMode{0};
    int32_t environmentEnabled{0};
    int32_t environmentWidth{0};
    int32_t environmentHeight{0};

    float environmentIntensity{1.0f};
    uint32_t visibilityMask{0xFFu};
    uint32_t pad0{0};
    uint32_t pad1{0};
};

static_assert(sizeof(PathTracedIntegratorParams) == 64);
static_assert(std::is_standard_layout_v<PathTracedIntegratorParams>);
static_assert(offsetof(PathTracedIntegratorParams, seed) == 16);
static_assert(offsetof(PathTracedIntegratorParams, samplingMode) == 32);
static_assert(offsetof(PathTracedIntegratorParams, environmentIntensity) == 48);
static_assert(offsetof(PathTracedIntegratorParams, visibilityMask) == 52);

// Mirrors PathTracedSceneAddresses in Shaders/PathTracer/Core/scene-addresses.part.glsl.
struct PathTracedSceneAddresses {
    VkDeviceAddress instances{0};
    VkDeviceAddress materials{0};
    VkDeviceAddress lights{0}; // Null when the view has no analytic lights.
    VkDeviceAddress environmentCdf{0};
    uint32_t energyCompensation{0};
    uint32_t pad0{0};
};

static_assert(sizeof(PathTracedSceneAddresses) == 40);
static_assert(std::is_standard_layout_v<PathTracedSceneAddresses>);
static_assert(offsetof(PathTracedSceneAddresses, instances) == 0);
static_assert(offsetof(PathTracedSceneAddresses, materials) == 8);
static_assert(offsetof(PathTracedSceneAddresses, lights) == 16);
static_assert(offsetof(PathTracedSceneAddresses, environmentCdf) == 24);
static_assert(offsetof(PathTracedSceneAddresses, energyCompensation) == 32);

// PathTracedInstance::flags. Must match the kInstance* constants in
// Shaders/PathTracer/Core/instance.part.glsl.
inline constexpr uint32_t kPathTracedInstanceTwoSidedShading = 1u << 0;

// One record per TLAS instance. Must match PathTracedInstance in Shaders/PathTracer/Core/instance.part.glsl.
struct PathTracedInstance {
    VkDeviceAddress positions{0};
    VkDeviceAddress attributes{0};
    VkDeviceAddress triangles{0};
    VkDeviceAddress aliasTable{0}; // Null unless the shape is an area light.
    int32_t materialIndex{-1};
    int32_t lightId{-1};
    uint32_t materialTextureOffset{kInvalidMaterialTextureOffset};
    uint32_t flags{0};
};

static_assert(sizeof(PathTracedInstance) == 48);
static_assert(std::is_standard_layout_v<PathTracedInstance>);
static_assert(offsetof(PathTracedInstance, positions) == 0);
static_assert(offsetof(PathTracedInstance, attributes) == 8);
static_assert(offsetof(PathTracedInstance, triangles) == 16);
static_assert(offsetof(PathTracedInstance, aliasTable) == 24);
static_assert(offsetof(PathTracedInstance, materialIndex) == 32);
static_assert(offsetof(PathTracedInstance, lightId) == 36);
static_assert(offsetof(PathTracedInstance, materialTextureOffset) == 40);
static_assert(offsetof(PathTracedInstance, flags) == 44);

struct PathTracerInstance {
    const Geometry* geometry{nullptr};
    glm::mat4 transform{1.0f};
    uint32_t triangleCount{0};
    uint32_t customIndex{0};
    uint32_t sceneIndex{0};
    uint8_t visibilityMask{0xFF};
};

struct PathTracerShaderStage {
    std::string_view name;
    VkRayTracingShaderGroupTypeKHR groupType{VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR};
};

struct PathTracerCreateInfo {
    std::string debugName;
    std::span<const PathTracerShaderStage> shaderStages;
    uint32_t resourceHeapSlotCount{kPathTracerFirstFreeSlot};
    uint32_t samplerHeapSlotCount{1};
    uint32_t integratorParamsSize{0};
};

class PathTracer {
public:
    PathTracer(
        Renderer& renderer, const PathTracerCreateInfo& createInfo, std::span<const PathTracerInstance> instances);
    ~PathTracer();

    PathTracer(const PathTracer&) = delete;
    PathTracer& operator=(const PathTracer&) = delete;
    PathTracer(PathTracer&&) = delete;
    PathTracer& operator=(PathTracer&&) = delete;

    VulkanResourceHeap& getResourceHeap() {
        return *m_resourceHeap;
    }

    VulkanSamplerHeap& getSamplerHeap() {
        return *m_samplerHeap;
    }

    void setStorageImage(const VulkanImageView& imageView);

    void setSceneIndex(uint32_t sceneIndex);

    uint32_t getSceneCount() const {
        return static_cast<uint32_t>(m_topLevelAccelStructures.size());
    }

    void updateCamera(const CameraParameters& cameraParams);

    void resetAccumulation();

    int32_t getFrameIndex() const {
        return m_frameIndex;
    }

    int32_t getAccumulatedSampleCount() const {
        return m_accumulatedSampleCount;
    }

    void uploadFrameData(const FrameContext& frameContext, std::span<const std::byte> integratorParams);

    void trace(const FrameContext& frameContext, VkExtent2D extent, std::span<const std::byte> pushData);

    // Call once per dispatch, after trace, with the number of samples that dispatch drew per pixel.
    void advance(int32_t sampleCount);

private:
    void createAccelerationStructures(std::span<const PathTracerInstance> instances, const std::string& debugName);
    void createPipeline(const PathTracerCreateInfo& createInfo);

    Renderer* m_renderer;

    std::vector<std::unique_ptr<VulkanAccelerationStructure>> m_bottomLevelAccelStructures;
    std::vector<std::unique_ptr<VulkanAccelerationStructure>> m_topLevelAccelStructures;
    uint32_t m_sceneIndex{0};

    std::unique_ptr<VulkanResourceHeap> m_resourceHeap;
    std::unique_ptr<VulkanSamplerHeap> m_samplerHeap;
    std::unique_ptr<VulkanPipeline> m_pipeline;
    ShaderBindingTable m_shaderBindingTable;

    std::unique_ptr<VulkanBuffer> m_cameraBuffer;
    std::unique_ptr<VulkanBuffer> m_integratorBuffer;
    uint32_t m_integratorParamsSize{0};

    CameraParameters m_cameraParams{};
    int32_t m_frameIndex{0};
    int32_t m_accumulatedSampleCount{0};
};

// Every integrator uploads its parameter block and its scene addresses as opaque bytes, so the layout stays the
// shader's business rather than PathTracer's.
template <typename T>
std::span<const std::byte> structAsBytes(const T& value) {
    static_assert(std::is_standard_layout_v<T>, "Only a standard-layout struct has a defined byte image.");
    return std::span<const std::byte>{reinterpret_cast<const std::byte*>(&value), sizeof(value)}; // NOLINT
}

// Writes the GGX directional-albedo table and its sampler into the tracer's heaps. The caller owns the image:
// the heap holds a plain view, so it has to outlive the tracer.
std::unique_ptr<VulkanImage> bindGgxAlbedoLut(Renderer& renderer, PathTracer& pathTracer);

} // namespace crisp
