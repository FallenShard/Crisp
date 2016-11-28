#include "VulkanRenderer.hpp"

#define NOMINMAX

#include "IO/FileUtils.hpp"
#include "IO/ShaderLoader.hpp"

#include "Pipelines/FullScreenQuadPipeline.hpp"

#include <glm/glm.hpp>

#include <iostream>

namespace crisp
{
    VulkanRenderer::VulkanRenderer(SurfaceCreator surfCreatorCallback, std::vector<const char*>&& extensions)
        : m_depthImageArray(VK_NULL_HANDLE)
        , m_depthImageViews(NumVirtualFrames, VK_NULL_HANDLE)
        , m_framesRendered(0)
        , m_currentFrameIndex(0)
        , m_displayedImageView(VK_NULL_HANDLE)
        , m_displayedImageLayerCount(0)
    {
        m_context = std::make_unique<VulkanContext>(surfCreatorCallback, std::forward<std::vector<const char*>>(extensions));
        m_device = std::make_unique<VulkanDevice>(m_context.get());
        m_swapChain = std::make_unique<VulkanSwapChain>(*m_context, *m_device);
        m_defaultRenderPass = std::make_unique<DefaultRenderPass>(this);

        VkFormat depthFormat = m_context->findSupportedDepthFormat();
        m_depthImageArray = m_device->createDeviceImageArray(m_swapChain->getExtent().width, m_swapChain->getExtent().height, NumVirtualFrames, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        m_device->transitionImageLayout(m_depthImageArray, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, NumVirtualFrames);
        for (unsigned int i = 0; i < NumVirtualFrames; i++)
            m_depthImageViews[i] = m_device->createImageView(m_depthImageArray, VK_IMAGE_VIEW_TYPE_2D, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, i, 1);
        

        for (auto& frameRes : m_frameResources)
        {
            VkCommandBufferAllocateInfo cmdBufallocInfo = {};
            cmdBufallocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cmdBufallocInfo.commandPool = m_device->getCommandPool();
            cmdBufallocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cmdBufallocInfo.commandBufferCount = 1;
            vkAllocateCommandBuffers(m_device->getHandle(), &cmdBufallocInfo, &frameRes.cmdBuffer);

            VkFenceCreateInfo fenceInfo = {};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.pNext = nullptr;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            vkCreateFence(m_device->getHandle(), &fenceInfo, nullptr, &frameRes.bufferFinishedFence);

            frameRes.imageAvailableSemaphore = m_device->createSemaphore();
            frameRes.renderFinishedSemaphore = m_device->createSemaphore();
        }

        std::string searchDir = "Resources/Shaders/Vulkan/";
        auto files = FileUtils::enumerateFiles(searchDir, "spv");

        ShaderLoader loader;
        for (auto& file : files)
        {
            auto shader = loader.load(m_device->getHandle(), file);
            auto shaderKey = file.substr(0, file.length() - 4);
            m_shaderModules.insert_or_assign(shaderKey, shader);
        }

        m_fsQuadPipeline = std::make_unique<FullScreenQuadPipeline>(this, &getDefaultRenderPass());
        m_fsQuadDescSet = m_fsQuadPipeline->allocateDescriptorSet(0);
        // create sampler
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        vkCreateSampler(m_device->getHandle(), &samplerInfo, nullptr, &m_sampler);

        // create vertex buffer
        std::vector<glm::vec2> vertices =
        {
            { -1.0f, -1.0f },
            { +1.0f, -1.0f },
            { +1.0f, +1.0f },
            { -1.0f, +1.0f }
        };
        m_fsQuadVertexBuffer = m_device->createDeviceBuffer(sizeof(glm::vec2) * vertices.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        m_device->fillDeviceBuffer(m_fsQuadVertexBuffer, vertices.data(), sizeof(glm::vec2) * vertices.size());

        // create index buffer
        std::vector<glm::u16vec3> faces =
        {
            { 0, 1, 2 },
            { 0, 2, 3 }
        };
        m_fsQuadIndexBuffer = m_device->createDeviceBuffer(sizeof(glm::u16vec3) * faces.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        m_device->fillDeviceBuffer(m_fsQuadIndexBuffer, faces.data(), sizeof(glm::u16vec3) * faces.size());

        VkDescriptorImageInfo imageInfo = {};
        imageInfo.sampler = m_sampler;

        std::array<VkWriteDescriptorSet, 1> descriptorWrites = {};
        descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet          = m_fsQuadDescSet;
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo      = &imageInfo;

        vkUpdateDescriptorSets(m_device->getHandle(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        registerRenderPass(DefaultRenderPassId, m_defaultRenderPass.get());
        
    }

    VulkanRenderer::~VulkanRenderer()
    {
        for (auto& frameRes : m_frameResources)
        {
            vkDestroyFence(m_device->getHandle(), frameRes.bufferFinishedFence, nullptr);
            vkDestroySemaphore(m_device->getHandle(), frameRes.imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(m_device->getHandle(), frameRes.renderFinishedSemaphore, nullptr);
            if (frameRes.framebuffer != VK_NULL_HANDLE)
                vkDestroyFramebuffer(m_device->getHandle(), frameRes.framebuffer, nullptr);
        }

        for (auto& view : m_depthImageViews)
            vkDestroyImageView(m_device->getHandle(), view, nullptr);

        for (auto& shaderModule : m_shaderModules)
        {
            vkDestroyShaderModule(m_device->getHandle(), shaderModule.second, nullptr);
        }
    }

    VulkanContext& VulkanRenderer::getContext()
    {
        return *m_context;
    }

    VulkanDevice& VulkanRenderer::getDevice()
    {
        return *m_device;
    }

    VulkanSwapChain& VulkanRenderer::getSwapChain()
    {
        return *m_swapChain;
    }

    VkCommandBuffer VulkanRenderer::acquireCommandBuffer()
    {
        vkWaitForFences(m_device->getHandle(), 1, &m_frameResources[m_currentFrameIndex].bufferFinishedFence, VK_TRUE, std::numeric_limits<uint64_t>::max());
        vkResetFences(m_device->getHandle(), 1, &m_frameResources[m_currentFrameIndex].bufferFinishedFence);
        return m_frameResources[m_currentFrameIndex].cmdBuffer;
    }

    uint32_t VulkanRenderer::acquireNextImageIndex()
    {
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(m_device->getHandle(), m_swapChain->getHandle(), std::numeric_limits<uint64_t>::max(), m_frameResources[m_currentFrameIndex].imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapChain();
            return -1;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            std::cerr << "Unable to acquire optimal swapchain image!\n";
            return -1;
        }

        return imageIndex;
    }

    void VulkanRenderer::resetCommandBuffer(VkCommandBuffer cmdBuffer)
    {
        vkResetCommandBuffer(cmdBuffer, 0);
    }

    VkFramebuffer VulkanRenderer::recreateFramebuffer(uint32_t swapChainImageViewIndex)
    {
        if (m_frameResources[m_currentFrameIndex].framebuffer != VK_NULL_HANDLE)
            vkDestroyFramebuffer(m_device->getHandle(), m_frameResources[m_currentFrameIndex].framebuffer, nullptr);

        std::vector<VkImageView> attachmentViews =
        {
            m_swapChain->getImageView(swapChainImageViewIndex),
            m_depthImageViews[m_currentFrameIndex]
        };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_defaultRenderPass->getHandle();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
        framebufferInfo.pAttachments = attachmentViews.data();
        framebufferInfo.width  = m_swapChain->getExtent().width;
        framebufferInfo.height = m_swapChain->getExtent().height;
        framebufferInfo.layers = 1;
        vkCreateFramebuffer(m_device->getHandle(), &framebufferInfo, nullptr, &m_frameResources[m_currentFrameIndex].framebuffer);

        return m_frameResources[m_currentFrameIndex].framebuffer;
    }

    void VulkanRenderer::submitToQueue(VkCommandBuffer cmdBuffer)
    {
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_frameResources[m_currentFrameIndex].imageAvailableSemaphore;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_frameResources[m_currentFrameIndex].cmdBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_frameResources[m_currentFrameIndex].renderFinishedSemaphore;
        vkQueueSubmit(m_device->getGraphicsQueue(), 1, &submitInfo, m_frameResources[m_currentFrameIndex].bufferFinishedFence);
    }

    void VulkanRenderer::present(uint32_t swapChainImageIndex)
    {
        VkSwapchainKHR swapChains[] = { m_swapChain->getHandle() };
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_frameResources[m_currentFrameIndex].renderFinishedSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &swapChainImageIndex;
        presentInfo.pResults = nullptr;
        auto result = vkQueuePresentKHR(m_device->getPresentQueue(), &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS)
        {
            throw std::exception("Failed to present swap chain image!");
        }
    }

    void VulkanRenderer::addDrawAction(std::function<void(VkCommandBuffer&)> drawAction, uint8_t renderPassId)
    {
        m_renderPasses.at(renderPassId).second.emplace_back(drawAction);
    }

    void VulkanRenderer::addCopyAction(std::function<void(VkCommandBuffer&)> copyAction)
    {
        m_copyActions.emplace_back(copyAction);
    }

    void VulkanRenderer::record(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer)
    {
        addDrawAction([this](VkCommandBuffer& commandBuffer)
        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_fsQuadPipeline->getPipeline());
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_fsQuadPipeline->getPipelineLayout(),
                0, 1, &m_fsQuadDescSet, 0, nullptr);

            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_fsQuadVertexBuffer, &offset);
            vkCmdBindIndexBuffer(commandBuffer, m_fsQuadIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

            unsigned int pushConst = m_currentFrameIndex;
            vkCmdPushConstants(commandBuffer, m_fsQuadPipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(unsigned int), &pushConst);
            vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);
        }, DefaultRenderPassId);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        for (auto& action : m_copyActions)
            action(commandBuffer);

        for (auto& item : m_renderPasses)
        {
            auto pass = item.second.first;
            pass->begin(commandBuffer, framebuffer);

            for (auto& action : item.second.second)
                action(commandBuffer);

            pass->end(commandBuffer);
        }

        vkEndCommandBuffer(commandBuffer);
    }

    void VulkanRenderer::displayImage(VkImageView imageView, int numLayers)
    {
        if (m_displayedImageView != VK_NULL_HANDLE)
            vkDestroyImageView(m_device->getHandle(), m_displayedImageView, nullptr);

        m_displayedImageLayerCount = numLayers;
        m_displayedImageView = imageView;
        
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_displayedImageView;
        
        std::array<VkWriteDescriptorSet, 1> descriptorWrites = {};
        descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet          = m_fsQuadDescSet;
        descriptorWrites[0].dstBinding      = 1;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo      = &imageInfo;
        
        vkUpdateDescriptorSets(m_device->getHandle(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    void VulkanRenderer::flushAllQueuedCopyRequests()
    {
    }

    uint32_t VulkanRenderer::getCurrentFrameIndex() const
    {
        return m_currentFrameIndex;
    }

    void VulkanRenderer::drawFrame()
    {
        auto cmdBuffer = acquireCommandBuffer();
        auto swapChainImgIndex = acquireNextImageIndex();
        if (swapChainImgIndex == -1)
            return;

        resetCommandBuffer(cmdBuffer);

        auto framebuffer = recreateFramebuffer(swapChainImgIndex);
        record(cmdBuffer, framebuffer);
        submitToQueue(cmdBuffer);
        present(swapChainImgIndex);

        m_copyActions.clear();
        for (auto& item : m_renderPasses)
            item.second.second.clear();
        m_currentFrameIndex = (m_currentFrameIndex + 1) % NumVirtualFrames;
    }

    void VulkanRenderer::recreateSwapChain()
    {
        vkDeviceWaitIdle(m_device->getHandle());

        m_swapChain->recreate(*m_context, *m_device);
        m_defaultRenderPass->recreate();

        VkFormat depthFormat = m_context->findSupportedDepthFormat();
        if (m_depthImageArray != VK_NULL_HANDLE)
        {
            m_device->destroyDeviceImage(m_depthImageArray);
        }
        m_depthImageArray = m_device->createDeviceImageArray(m_swapChain->getExtent().width, m_swapChain->getExtent().height, NumVirtualFrames, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        m_device->transitionImageLayout(m_depthImageArray, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, NumVirtualFrames);
        for (unsigned int i = 0; i < NumVirtualFrames; i++)
        {
            if (m_depthImageViews[i] != VK_NULL_HANDLE)
            {
                vkDestroyImageView(m_device->getHandle(), m_depthImageViews[i], nullptr);
            }
            m_depthImageViews[i] = m_device->createImageView(m_depthImageArray, VK_IMAGE_VIEW_TYPE_2D, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, i, 1);
        }
    }

    void VulkanRenderer::finish()
    {
        vkDeviceWaitIdle(m_device->getHandle());
    }

    void VulkanRenderer::resize(int width, int height)
    {
        recreateSwapChain();
        m_fsQuadPipeline->resize(width, height);
    }

    void VulkanRenderer::registerRenderPass(uint8_t key, VulkanRenderPass* renderPass)
    {
        m_renderPasses[key] = std::make_pair(renderPass, ActionVector());
    }

    void VulkanRenderer::unregisterRenderPass(uint8_t key)
    {
        m_renderPasses.erase(key);
    }

    VulkanRenderPass& VulkanRenderer::getDefaultRenderPass() const
    {
        return *m_defaultRenderPass;
    }

    VkShaderModule VulkanRenderer::getShaderModule(std::string&& key) const
    {
        return m_shaderModules.at(key);
    }

    VkExtent2D VulkanRenderer::getSwapChainExtent() const
    {
        return m_swapChain->getExtent();
    }
}