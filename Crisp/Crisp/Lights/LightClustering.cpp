#include <Crisp/Lights/LightClustering.hpp>

#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/RenderGraph.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

#pragma warning(disable : 26451) // Arithmetic overflow.

namespace crisp {
namespace {
constexpr const char* kLightCullingPass = "lightCullingPass";

struct Tile {
    std::array<glm::vec3, 4> screenSpacePoints;
    std::array<glm::vec3, 4> viewSpacePoints;
};

glm::vec4 computePlaneFromSpan(const glm::vec3& a, const glm::vec3& b) {
    glm::vec3 n = glm::normalize(glm::cross(a, b));
    return {n, glm::dot(n, a)};
}

std::unique_ptr<VulkanPipeline> createLightCullingComputePipeline(Renderer* renderer, const VkExtent3D& workGroupSize) {
    return createComputePipeline(*renderer, "light-culling.comp", workGroupSize, [](PipelineLayoutBuilder& builder) {
        builder.setDescriptorDynamic(0, 1, true);
        builder.setDescriptorDynamic(0, 2, true);
        builder.setDescriptorDynamic(0, 3, true);
        builder.setDescriptorDynamic(0, 4, true);
        builder.setDescriptorSetBuffering(1, true);
    });
}
} // namespace

glm::ivec2 calculateTileGridDims(glm::ivec2 tileSize, glm::ivec2 screenSize) {
    return (glm::ivec2(screenSize) - glm::ivec2(1)) / tileSize + glm::ivec2(1);
}

std::vector<TileFrustum> createTileFrusta(
    glm::ivec2 tileSize, glm::ivec2 screenSize, const glm::mat4& projectionMatrix) {
    glm::ivec2 numTiles = calculateTileGridDims(tileSize, screenSize);
    uint32_t tileCount = numTiles.x * numTiles.y;

    std::vector<TileFrustum> tilePlanes(tileCount);
    for (int j = 0; j < numTiles.y; ++j) {
        for (int i = 0; i < numTiles.x; ++i) {
            Tile tile{};
            for (int k = 0; k < 4; ++k) {
                const float x = static_cast<float>(tileSize.x) * static_cast<float>(i + k % 2);
                const float y = static_cast<float>(tileSize.y) * static_cast<float>(j + k / 2); // NOLINT
                tile.screenSpacePoints[k] = glm::vec3(x, y, 1.0f);

                glm::vec4 ndc = glm::vec4(tile.screenSpacePoints[k], 1.0f);
                ndc.x /= static_cast<float>(screenSize.x); // in [0, 1]
                ndc.y /= static_cast<float>(screenSize.y);
                ndc.x = ndc.x * 2.0f - 1.0f; // in [-1, 1]
                ndc.y = ndc.y * 2.0f - 1.0f;

                glm::vec4 view = glm::inverse(projectionMatrix) * ndc;
                tile.viewSpacePoints[k] = view / view.w;
            }

            tilePlanes[j * numTiles.x + i].frustumPlanes[0] =
                computePlaneFromSpan(tile.viewSpacePoints[1], tile.viewSpacePoints[0]);
            tilePlanes[j * numTiles.x + i].frustumPlanes[1] =
                computePlaneFromSpan(tile.viewSpacePoints[3], tile.viewSpacePoints[1]);
            tilePlanes[j * numTiles.x + i].frustumPlanes[2] =
                computePlaneFromSpan(tile.viewSpacePoints[2], tile.viewSpacePoints[3]);
            tilePlanes[j * numTiles.x + i].frustumPlanes[3] =
                computePlaneFromSpan(tile.viewSpacePoints[0], tile.viewSpacePoints[2]);
        }
    }

    return tilePlanes;
}

void addToRenderGraph(
    Renderer* renderer,
    RenderGraph& renderGraph,
    const LightClustering& lightClustering,
    const VulkanRingBuffer& cameraBuffer,
    const VulkanRingBuffer& pointLightBuffer) {
    auto& cullingPass = renderGraph.addComputePass(kLightCullingPass);
    cullingPass.workGroupSize = {
        static_cast<uint32_t>(lightClustering.m_tileSize.x), static_cast<uint32_t>(lightClustering.m_tileSize.y), 1};
    cullingPass.numWorkGroups = {
        static_cast<uint32_t>(lightClustering.m_gridSize.x), static_cast<uint32_t>(lightClustering.m_tileSize.y), 1};
    cullingPass.pipeline = createLightCullingComputePipeline(renderer, cullingPass.workGroupSize);
    cullingPass.material = std::make_unique<Material>(cullingPass.pipeline.get());
    cullingPass.material->writeDescriptor(0, 0, lightClustering.m_tilePlaneBuffer->getDescriptorInfo());
    cullingPass.material->writeDescriptor(0, 1, lightClustering.m_lightIndexCountBuffer->getDescriptorInfo());
    cullingPass.material->writeDescriptor(0, 2, pointLightBuffer.getDescriptorInfo());
    cullingPass.material->writeDescriptor(0, 3, cameraBuffer.getDescriptorInfo());
    cullingPass.material->writeDescriptor(0, 4, lightClustering.m_lightIndexListBuffer->getDescriptorInfo());
    cullingPass.material->writeDescriptor(
        1, 0, lightClustering.m_lightGridView->getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    // cullingPass.material->setDynamicBufferView(0, *lightClustering.m_lightIndexCountBuffer, 0);
    // cullingPass.material->setDynamicBufferView(1, pointLightBuffer, 0);
    // cullingPass.material->setDynamicBufferView(2, cameraBuffer, 0);
    // cullingPass.material->setDynamicBufferView(3, *lightClustering.m_lightIndexListBuffer, 0);
    cullingPass.preDispatchCallback =
        [lightCountBuffer = lightClustering.m_lightIndexCountBuffer.get()](
            RenderGraph::Node& /*node*/, VulkanCommandBuffer& cmdBuffer, uint32_t frameIndex) {
            // Before culling can start, zero out the light index count buffer
            glm::uvec4 zero(0);

            VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            barrier.buffer = lightCountBuffer->getHandle();
            barrier.offset = frameIndex * sizeof(zero);
            barrier.size = sizeof(zero);

            vkCmdUpdateBuffer(cmdBuffer.getHandle(), barrier.buffer, barrier.offset, barrier.size, &zero);
            cmdBuffer.insertBufferMemoryBarrier(
                barrier, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        };

    // m_renderGraph->addDependency(DepthPrePass, LightCullingPass, 0);
    renderGraph.addDependency(
        kLightCullingPass,
        "MainPass",
        [lightIndexBuffer = lightClustering.m_lightIndexListBuffer.get()](
            const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
            VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            auto info = lightIndexBuffer->getDescriptorInfo();
            barrier.buffer = info.buffer;
            barrier.offset = info.offset;
            barrier.size = info.range;

            cmdBuffer.insertBufferMemoryBarrier(
                barrier, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });
}

void LightClustering::configure(
    Renderer* renderer, const CameraParameters& cameraParameters, const uint32_t maximumLightCount) {
    m_tileSize = glm::ivec2(16);
    m_gridSize = calculateTileGridDims(m_tileSize, cameraParameters.screenSize);
    const auto tileFrusta{createTileFrusta(m_tileSize, cameraParameters.screenSize, cameraParameters.P)};
    const std::size_t tileCount = tileFrusta.size();

    m_tilePlaneBuffer =
        createStorageRingBuffer(&renderer->getDevice(), tileCount * sizeof(TileFrustum), tileFrusta.data());
    m_lightIndexCountBuffer = createStorageRingBuffer(&renderer->getDevice(), sizeof(uint32_t));
    m_lightIndexListBuffer =
        createStorageRingBuffer(&renderer->getDevice(), tileCount * sizeof(uint32_t) * maximumLightCount);

    m_lightGrid = createSampledStorageImage(
        *renderer,
        VK_FORMAT_R32G32_UINT,
        VkExtent3D{static_cast<uint32_t>(m_gridSize.x), static_cast<uint32_t>(m_gridSize.y), 1u});
    m_lightGridView = createView(renderer->getDevice(), *m_lightGrid, VK_IMAGE_VIEW_TYPE_2D);

    renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer) {
        m_lightGrid->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_GENERAL, kNullStage >> kComputeRead);
    });
}

} // namespace crisp
