#include "StaticPicture.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <algorithm>

#include "IO/Image.hpp"

#include "Vulkan/VulkanRenderer.hpp"
#include "Vulkan/VulkanSwapChain.hpp"
#include "Vulkan/Pipelines/FullScreenQuadPipeline.hpp"

namespace crisp
{
    StaticPicture::StaticPicture(std::string fileName, VkFormat format, VulkanRenderer& renderer)
    {
        m_renderer = &renderer;
        m_pipeline = std::make_unique<FullScreenQuadPipeline>(m_renderer, &m_renderer->getDefaultRenderPass());

        Image image(fileName);

        // create texture image
        unsigned int numChannels = 4;
        m_extent.width  = image.getWidth();
        m_extent.height = image.getHeight();
        m_aspectRatio = static_cast<float>(m_extent.width) / m_extent.height;
        auto byteSize = m_extent.width * m_extent.height * numChannels * sizeof(unsigned char);

        m_viewport.minDepth = 0;
        m_viewport.maxDepth = 1;
        resize(m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height);

        m_tex = renderer.getDevice().createDeviceImage(m_extent.width, m_extent.height, format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        renderer.getDevice().fillDeviceImage(m_tex, image.getData(), byteSize, { m_extent.width, m_extent.height, 1u }, 1);
        renderer.getDevice().transitionImageLayout(m_tex, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);

        // create view
        m_texView = renderer.getDevice().createImageView(m_tex, VK_IMAGE_VIEW_TYPE_2D_ARRAY, format, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);

        // create sampler
        m_vkSampler = renderer.getDevice().createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        // descriptor set
        m_descriptorSet = m_pipeline->allocateDescriptorSet(0);

        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = m_texView;
        imageInfo.sampler     = m_vkSampler;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};
        descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet          = m_descriptorSet;
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo      = &imageInfo;

        descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet          = m_descriptorSet;
        descriptorWrites[1].dstBinding      = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo      = &imageInfo;

        vkUpdateDescriptorSets(m_renderer->getDevice().getHandle(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        m_drawItem.pipeline            = m_pipeline->getHandle();
        m_drawItem.pipelineLayout      = m_pipeline->getPipelineLayout();
        m_drawItem.descriptorSetOffset = 0;
        m_drawItem.descriptorSets.push_back(m_descriptorSet);
    }

    StaticPicture::~StaticPicture()
    {
        vkDestroySampler(m_renderer->getDevice().getHandle(), m_vkSampler, nullptr);
        vkDestroyImageView(m_renderer->getDevice().getHandle(), m_texView, nullptr);
    }

    void StaticPicture::resize(int width, int height)
    {
        m_pipeline->resize(width, height);

        float screenAspect = static_cast<float>(width) / height;
        if (screenAspect > m_aspectRatio)
        {
            m_viewport.height = static_cast<float>(height);
            m_viewport.width = static_cast<float>(height * m_aspectRatio);
            m_viewport.x = (width - m_viewport.width) / 2;
            m_viewport.y = 0;
        }
        else
        {
            m_viewport.height = static_cast<float>(width / m_aspectRatio);
            m_viewport.width = static_cast<float>(width);
            m_viewport.x = 0;
            m_viewport.y = (height - m_viewport.height) / 2;
        }
    }

    void StaticPicture::draw()
    {
        m_renderer->addDrawAction([this](VkCommandBuffer& cmdBuffer)
        {
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getHandle());
            vkCmdSetViewport(cmdBuffer, 0, 1, &m_viewport);
            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getPipelineLayout(),
                0, 1, &m_descriptorSet, 0, nullptr);

            unsigned int pushConst = 0;
            vkCmdPushConstants(cmdBuffer, m_pipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(unsigned int), &pushConst);

            m_renderer->drawFullScreenQuad(cmdBuffer);
        }, VulkanRenderer::DefaultRenderPassId);
    }
}