#include <Crisp/Models/Skybox.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Image/Io/Utils.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

namespace crisp {
constexpr std::size_t NumCubeMapFaces = 6;
const std::array<const std::string, NumCubeMapFaces> SideFilenames = {"left", "right", "top", "bottom", "back", "front"};

Skybox::Skybox(Renderer* renderer, const VulkanRenderPass& renderPass, const std::string& cubeMapFolder)
    : m_cubeGeometry(createGeometry(
          *renderer,
          loadTriangleMesh(renderer->getResourcesPath() / "Meshes/cube.obj", flatten(kPosVertexFormat)).unwrap(),
          kPosVertexFormat))
    , m_transformPack{} {
    m_transformBuffer = createUniformRingBuffer(&renderer->getDevice(), sizeof(TransformPack));

    const std::filesystem::path cubeMapDir = renderer->getResourcesPath() / "Textures/Cubemaps" / cubeMapFolder;

    std::vector<Image> cubeMapImages;
    cubeMapImages.reserve(NumCubeMapFaces);
    for (std::size_t i = 0; i < NumCubeMapFaces; ++i) {
        cubeMapImages.push_back(loadImage(cubeMapDir / (SideFilenames[i] + ".jpg")).unwrap());
    }

    const uint32_t width = cubeMapImages[0].getWidth();
    const uint32_t height = cubeMapImages[0].getHeight();

    m_cubeMap = std::make_unique<VulkanImage>(
        renderer->getDevice(),
        VkExtent3D{width, height, 1u},
        static_cast<uint32_t>(NumCubeMapFaces),
        1,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

    for (uint32_t i = 0; i < NumCubeMapFaces; ++i) {
        fillImageLayer(*m_cubeMap, *renderer, cubeMapImages[i].getData(), width * height * 4, i);
    }

    m_cubeMapView = createView(
        renderer->getDevice(), *m_cubeMap, VK_IMAGE_VIEW_TYPE_CUBE, 0, static_cast<uint32_t>(cubeMapImages.size()));
    m_sampler = createLinearClampSampler(renderer->getDevice());
    m_pipeline = renderer->createPipeline("Skybox.json", renderPass, 0);
    updateRenderNode(*m_sampler, *m_cubeMapView);

    // renderer->getDebugMarker().setObjectName(m_cubeMapView->getHandle(), "Cube Map View");

    renderer->getDevice().flushDescriptorUpdates();
}

Skybox::Skybox(
    Renderer* renderer,
    const VulkanRenderPass& renderPass,
    const VulkanImageView& cubeMapView,
    const VulkanSampler& sampler)
    : m_cubeGeometry(createGeometry(
          *renderer,
          loadTriangleMesh(renderer->getResourcesPath() / "Meshes/cube.obj", flatten(kPosVertexFormat)).unwrap(),
          kPosVertexFormat))
    , m_transformPack{} {
    m_transformBuffer = createUniformRingBuffer(&renderer->getDevice(), sizeof(TransformPack));

    m_pipeline = renderer->createPipeline("Skybox.json", renderPass, 0);
    updateRenderNode(sampler, cubeMapView);

    renderer->getDevice().flushDescriptorUpdates();
}

void Skybox::updateRenderNode(const VulkanSampler& sampler, const VulkanImageView& cubeMapView) {
    m_material = std::make_unique<Material>(m_pipeline.get());
    m_material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
    m_material->writeDescriptor(0, 1, cubeMapView.getDescriptorInfo(&sampler));

    m_renderNode.transformBuffer = m_transformBuffer.get();
    m_renderNode.transformPack = &m_transformPack;
    m_renderNode.transformHandle.index = 0;
    m_renderNode.geometry = &m_cubeGeometry;
    m_renderNode.pass(kForwardLightingPass).material = m_material.get();
}

void Skybox::updateTransforms(const glm::mat4& V, const glm::mat4& P, const uint32_t regionIndex) {
    m_transformPack.M = glm::mat4(1.0f);
    m_transformPack.MV = glm::mat4(glm::mat3(V));
    m_transformPack.MVP = P * m_transformPack.MV;

    m_transformBuffer->updateStagingBufferFromStruct(m_transformPack, regionIndex);
}

const RenderNode& Skybox::getRenderNode() {
    return m_renderNode;
}

VulkanImageView* Skybox::getSkyboxView() const {
    return m_cubeMapView.get();
}

void Skybox::updateDeviceBuffer(const VkCommandBuffer cmdBuffer) {
    m_transformBuffer->updateDeviceBuffer(cmdBuffer);
}

} // namespace crisp