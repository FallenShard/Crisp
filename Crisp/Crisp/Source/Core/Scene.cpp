#include "Scene.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include "vulkan/VulkanRenderer.hpp"
#include "vulkan/Pipelines/UniformColorPipeline.hpp"


namespace crisp
{
    Scene::Scene(VulkanRenderer* renderer)
        : m_renderer(renderer)
    {
        std::vector<glm::vec3> vertices;
        vertices.emplace_back(-0.5f, -0.5f, 0.0f);
        vertices.emplace_back(+0.5f, -0.5f, 0.0f);
        vertices.emplace_back(+0.5f, +0.5f, 0.0f);
        vertices.emplace_back(-0.5f, +0.5f, 0.0f);

        std::vector<glm::u16vec3> faces;
        faces.emplace_back(0, 1, 2);
        faces.emplace_back(0, 2, 3);

        m_pipeline = std::make_unique<UniformColorPipeline>(m_renderer, &m_renderer->getDefaultRenderPass());

        m_descSets.resize(2);
        m_descSets[0] = m_pipeline->allocateDescriptorSet(0);
        m_descSets[1] = m_pipeline->allocateDescriptorSet(1);

        auto& device = m_renderer->getDevice();

        m_buffer      = device.createDeviceBuffer(vertices.size() * sizeof(glm::vec3), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices.data());
        m_indexBuffer = device.createDeviceBuffer(faces.size() * sizeof(glm::u16vec3), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, faces.data());

        std::vector<glm::vec4> colors(16);
        colors.at(0) = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        colors.at(1) = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
        colors.at(2) = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        colors.at(3) = glm::vec4(0.0f, 0.5f, 0.0f, 1.0f);
        colors.at(4) = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
        colors.at(5) = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        colors.at(6) = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
        colors.at(7) = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
        colors.at(8) = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);
        colors.at(9) = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
        m_colorPaletteBuffer = device.createDeviceBuffer(colors.size() * sizeof(glm::vec4), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, colors.data());

        auto extent = m_renderer->getSwapChainExtent();
        auto aspect = static_cast<float>(extent.width) / extent.height;
        auto correction = glm::scale(glm::vec3(1.0f, -1.0f, 1.0f));
        auto proj = glm::perspective(glm::radians(45.0f), aspect, 1.0f, 100.0f);

        m_transforms.MV = glm::lookAt(glm::vec3(5.0f, 5.0f, 5.0f), { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
        m_transforms.MVP = correction * proj * m_transforms.MV;

        m_transformsBuffer = device.createDeviceBuffer(sizeof(Transforms), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &m_transforms);

        VkDescriptorBufferInfo transBufferInfo = {};
        transBufferInfo.buffer = m_transformsBuffer;
        transBufferInfo.offset = 0;
        transBufferInfo.range = sizeof(Transforms);

        VkDescriptorBufferInfo colorBufferInfo = {};
        colorBufferInfo.buffer = m_colorPaletteBuffer;
        colorBufferInfo.offset = 0;
        colorBufferInfo.range = sizeof(glm::vec4) * colors.size();

        std::vector<VkWriteDescriptorSet> descWrites(2, {});
        descWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[0].dstSet          = m_descSets[1];
        descWrites[0].dstBinding      = 0;
        descWrites[0].dstArrayElement = 0;
        descWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descWrites[0].descriptorCount = 1;
        descWrites[0].pBufferInfo     = &colorBufferInfo;

        descWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrites[1].dstSet          = m_descSets[0];
        descWrites[1].dstBinding      = 0;
        descWrites[1].dstArrayElement = 0;
        descWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        descWrites[1].descriptorCount = 1;
        descWrites[1].pBufferInfo     = &transBufferInfo;
        vkUpdateDescriptorSets(device.getHandle(), static_cast<uint32_t>(descWrites.size()), descWrites.data(), 0, nullptr);


    }

    Scene::~Scene()
    {

    }

    void Scene::update(double dt)
    {

    }

    void Scene::render()
    {
        m_renderer->addDrawAction([this](VkCommandBuffer& commandBuffer)
        {
            uint32_t transBufferOffset = 0;

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getHandle());
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getPipelineLayout(),
                0, static_cast<uint32_t>(m_descSets.size()), m_descSets.data(), 1, &transBufferOffset);

            int colorIndex = 0;
            vkCmdPushConstants(commandBuffer, m_pipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int), &colorIndex);

            VkDeviceSize vboOffset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_buffer, &vboOffset);
            vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);
            vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);

        }, VulkanRenderer::DefaultRenderPassId);
    }
}