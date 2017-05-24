#include "BackgroundImage.hpp"

#include "IO/ImageFileBuffer.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/VulkanRenderer.hpp"
#include "Renderer/Texture.hpp"
#include "Renderer/TextureView.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"

namespace crisp
{
    BackgroundImage::BackgroundImage(std::string fileName, VkFormat format, VulkanRenderer* renderer)
        : m_renderer(renderer)
    {
        auto imageBuffer = std::make_shared<ImageFileBuffer>(fileName);

        // create texture image
        m_extent.width  = imageBuffer->getWidth();
        m_extent.height = imageBuffer->getHeight();
        m_aspectRatio = static_cast<float>(m_extent.width) / m_extent.height;
        auto byteSize = m_extent.width * m_extent.height * imageBuffer->getNumComponents() * sizeof(unsigned char);

        m_viewport.minDepth = 0;
        m_viewport.maxDepth = 1;
        resize(m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height);

        m_texture = std::make_unique<Texture>(m_renderer, VkExtent3D{ m_extent.width, m_extent.height, 1u }, 1, format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        m_textureView = m_texture->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, 1);
        m_texture->fill(imageBuffer->getData(), byteSize);

        m_vkSampler = m_renderer->getDevice()->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        m_pipeline = std::make_unique<FullScreenQuadPipeline>(m_renderer, m_renderer->getDefaultRenderPass());
        m_descSetGroup = { m_pipeline->allocateDescriptorSet(0) };
        m_descSetGroup.postImageUpdate(0, 0, VK_DESCRIPTOR_TYPE_SAMPLER,       m_textureView->getDescriptorInfo(m_vkSampler));
        m_descSetGroup.postImageUpdate(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_textureView->getDescriptorInfo(m_vkSampler));
        m_descSetGroup.flushUpdates(m_renderer->getDevice());
    }

    BackgroundImage::~BackgroundImage()
    {
        vkDestroySampler(m_renderer->getDevice()->getHandle(), m_vkSampler, nullptr);
    }

    void BackgroundImage::resize(int width, int height)
    {
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

    void BackgroundImage::draw()
    {
        m_renderer->addDrawAction([this](VkCommandBuffer& cmdBuffer)
        {
            m_pipeline->bind(cmdBuffer);
            vkCmdSetViewport(cmdBuffer, 0, 1, &m_viewport);
            m_descSetGroup.bind(cmdBuffer, m_pipeline->getPipelineLayout());

            unsigned int pushConst = 0;
            vkCmdPushConstants(cmdBuffer, m_pipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(unsigned int), &pushConst);

            m_renderer->drawFullScreenQuad(cmdBuffer);
        }, VulkanRenderer::DefaultRenderPassId);
    }
}