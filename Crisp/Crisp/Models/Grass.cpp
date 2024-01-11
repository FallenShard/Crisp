#include <Crisp/Models/Grass.hpp>

#include <random>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanRenderPass.hpp>
#include <Crisp/Vulkan/VulkanSampler.hpp>

#include <Crisp/Vulkan/VulkanPipeline.hpp>

namespace crisp {
namespace {
static const int PC0 = 0;
}

Grass::Grass(
    Renderer* renderer,
    VulkanRenderPass* /*mainRenderPass*/,
    VulkanRenderPass* /*shadowRenderPass*/,
    UniformBuffer* /*cameraBuffer*/,
    VulkanSampler* /*sampler*/)
    : m_renderer(renderer) {
    // m_shadowPipelines.reserve(4);
    // for (uint32_t i = 0; i < 4; i++)
    //     m_shadowPipelines.emplace_back(createInstancingShadowMapPipeline(m_renderer, shadowRenderPass, i));
    // m_shadowMaterial = std::make_unique<Material>(m_shadowPipelines[0].get());
    ////m_renderer->getDevice()->postDescriptorWrite(m_shadowMaterial->makeDescriptorWrite(0, 0),
    /// csm->getLightTransformBuffer()->getDescriptorInfo());

    // std::default_random_engine randomEngine(std::random_device{}());
    // std::uniform_real_distribution<float> distrib(0.0f, 1.0f);

    // std::vector<glm::mat4> matrices;
    // for (int i = 0; i < 100; i++) {
    //     for (int j = 0; j < 100; j++) {
    //         float angle = distrib(randomEngine) * glm::radians(360.0f);
    //         glm::vec3 seed = glm::vec3(-float(i) * 0.25f + 10.0f, 0.0f, -float(j) * 0.25f + 10.0f);
    //         seed.x += distrib(randomEngine);
    //         seed.z += distrib(randomEngine);
    //         matrices.push_back(glm::translate(seed) * glm::rotate(angle, glm::vec3(0.0f, 1.0f, 0.0f)));
    //     }
    // }

    // VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    // auto instanceBuffer = std::make_unique<VulkanBuffer>(renderer->getDevice(), sizeof(glm::mat4) * matrices.size(),
    // usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); m_renderer->fillDeviceBuffer(instanceBuffer.get(), matrices);

    // m_bladeGeometry = std::make_unique<Geometry>(m_renderer, createGrassBlade({ VertexAttribute::Position }));
    // m_bladeGeometry->addVertexBuffer(std::move(instanceBuffer));
    // m_bladeGeometry->setInstanceCount(static_cast<uint32_t>(matrices.size()));

    // m_pipeline = createGrassPipeline(m_renderer, mainRenderPass);
    // m_material = std::make_unique<Material>(m_pipeline.get());
    // m_renderer->getDevice()->postDescriptorWrite(m_material->makeDescriptorWrite(0, 0),
    // cameraBuffer->getDescriptorInfo());
    // m_renderer->getDevice()->postDescriptorWrite(m_material->makeDescriptorWrite(0, 1),
    // csm->getLightTransformBuffer()->getDescriptorInfo()); for (uint32_t i = 0; i < kRendererVirtualFrameCount;
    // i++)
    //     for (int c = 0; c < 4; c++)
    //         m_renderer->getDevice()->postDescriptorWrite(m_material->makeDescriptorWrite(1, 0, c, i),
    //         shadowRenderPass->getRenderTargetView(c, i).getDescriptorInfo(sampler->getHandle()));

    // m_drawCommand.pipeline = m_pipeline.get();
    // m_drawCommand.dynamicBufferViews.push_back({ cameraBuffer, 0 });
    // m_drawCommand.dynamicBufferViews.push_back({ csm->getLightTransformBuffer(), 0 });
    // m_drawCommand.material = m_material.get();
    // m_drawCommand.geometry = m_bladeGeometry.get();
    // m_drawCommand.setGeometryView(m_bladeGeometry->createIndexedGeometryView());

    // m_shadowDrawCommand.pipeline = m_shadowPipelines[0].get();
    // m_shadowDrawCommand.dynamicBufferViews.push_back({ csm->getLightTransformBuffer(), 0 });
    // m_shadowDrawCommand.setPushConstantView(PC0);
    // m_shadowDrawCommand.material = m_shadowMaterial.get();
    // m_shadowDrawCommand.geometry = m_bladeGeometry.get();
    // m_shadowDrawCommand.setGeometryView(m_bladeGeometry->createIndexedGeometryView());
}

Grass::~Grass() {}

DrawCommand Grass::createDrawCommand() {
    return m_drawCommand;
}

DrawCommand Grass::createShadowCommand(uint32_t subpassIndex) {
    DrawCommand command = m_shadowDrawCommand;
    // TODO: command.setPushConstants(subpassIndex);
    command.pipeline = m_shadowPipelines[subpassIndex].get();
    return command;
}
} // namespace crisp