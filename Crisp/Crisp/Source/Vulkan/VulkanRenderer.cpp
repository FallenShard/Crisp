#include "VulkanRenderer.hpp"

#define NOMINMAX

#include "IO/FileUtils.hpp"

#include "Pipelines/FullScreenQuadPipeline.hpp"

#include "Math/Headers.hpp"

#include <iostream>

namespace crisp
{
    VulkanRenderer::VulkanRenderer(SurfaceCreator surfCreatorCallback, std::vector<const char*>&& extensions)
        : m_depthImageArray(VK_NULL_HANDLE)
        , m_depthImageViews(NumVirtualFrames, VK_NULL_HANDLE)
        , m_framesRendered(0)
        , m_currentFrameIndex(0)
    {
        // Create fundamental objects for the API
        m_context           = std::make_unique<VulkanContext>(surfCreatorCallback, std::forward<std::vector<const char*>>(extensions));
        m_device            = std::make_unique<VulkanDevice>(m_context.get());
        m_swapChain         = std::make_unique<VulkanSwapChain>(*m_context, *m_device);
        m_defaultRenderPass = std::make_unique<DefaultRenderPass>(this);

        // Depth images and views creation
        m_depthFormat = m_context->findSupportedDepthFormat();
        m_depthImageArray = m_device->createDeviceImageArray(m_swapChain->getExtent().width, m_swapChain->getExtent().height, NumVirtualFrames, m_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        m_device->transitionImageLayout(m_depthImageArray, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, NumVirtualFrames);
        for (unsigned int i = 0; i < NumVirtualFrames; i++)
            m_depthImageViews[i] = m_device->createImageView(m_depthImageArray, VK_IMAGE_VIEW_TYPE_2D, m_depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, i, 1);

        // Create frame resources, such as command buffer, fence and semaphores
        for (auto& frameRes : m_frameResources)
        {
            VkCommandBufferAllocateInfo cmdBufallocInfo = {};
            cmdBufallocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cmdBufallocInfo.commandPool        = m_device->getCommandPool();
            cmdBufallocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
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

        // Creates a map of all shaders
        std::string searchDir = "Resources/Shaders/";
        loadShaders(searchDir);

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

        m_fsQuadVertexBufferBindingGroup =
        {
            { m_fsQuadVertexBuffer, 0 }
        };

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
        {
            vkDestroyImageView(m_device->getHandle(), view, nullptr);
        }

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

    void VulkanRenderer::loadShaders(std::string dirPath)
    {
        auto files = FileUtils::enumerateFiles(dirPath, "spv");
        for (auto& file : files)
        {
            auto shaderCode = FileUtils::readBinaryFile(dirPath + file);

            VkShaderModuleCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = shaderCode.size();
            createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

            VkShaderModule shaderModule(VK_NULL_HANDLE);
            vkCreateShaderModule(m_device->getHandle(), &createInfo, nullptr, &shaderModule);

            auto shaderKey = file.substr(0, file.length() - 4);

            auto existingItem = m_shaderModules.find(shaderKey);
            if (existingItem != m_shaderModules.end())
            {
                vkDestroyShaderModule(m_device->getHandle(), existingItem->second, nullptr);
            }

            m_shaderModules.insert_or_assign(shaderKey, shaderModule);
        }
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
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = m_defaultRenderPass->getHandle();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
        framebufferInfo.pAttachments    = attachmentViews.data();
        framebufferInfo.width           = m_swapChain->getExtent().width;
        framebufferInfo.height          = m_swapChain->getExtent().height;
        framebufferInfo.layers          = 1;
        vkCreateFramebuffer(m_device->getHandle(), &framebufferInfo, nullptr, &m_frameResources[m_currentFrameIndex].framebuffer);

        return m_frameResources[m_currentFrameIndex].framebuffer;
    }

    void VulkanRenderer::submitToQueue(VkCommandBuffer)
    {
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &m_frameResources[m_currentFrameIndex].imageAvailableSemaphore;
        submitInfo.pWaitDstStageMask    = waitStages;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &m_frameResources[m_currentFrameIndex].cmdBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &m_frameResources[m_currentFrameIndex].renderFinishedSemaphore;
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

    void VulkanRenderer::addDrawAction(std::function<void(VkCommandBuffer&)> drawAction, uint32_t renderPassId)
    {
        m_renderPasses.at(renderPassId).second.emplace_back(drawAction);
    }

    void VulkanRenderer::addCopyAction(std::function<void(VkCommandBuffer&)> copyAction)
    {
        m_copyActions.emplace_back(copyAction);
    }

    void VulkanRenderer::addImageTransition(std::function<void(VkCommandBuffer&)> imageTransition)
    {
        m_imageTransitions.emplace_back(imageTransition);
    }

    void VulkanRenderer::record(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer)
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        for (auto& transition : m_imageTransitions)
            transition(commandBuffer);

        for (auto& action : m_copyActions)
            action(commandBuffer);

        for (auto& item : m_renderPasses)
        {
            auto pass = item.second.first;

            if (!item.second.second.empty())
            {
                pass->begin(commandBuffer, framebuffer);

                for (auto& action : item.second.second)
                    action(commandBuffer);

                pass->end(commandBuffer);
            }
        }

        vkEndCommandBuffer(commandBuffer);
    }

    uint32_t VulkanRenderer::getCurrentVirtualFrameIndex() const
    {
        return m_currentFrameIndex;
    }

    void VulkanRenderer::drawFrame()
    {
        auto cmdBuffer = acquireCommandBuffer();
        auto swapChainImgIndex = acquireNextImageIndex();
        if (swapChainImgIndex == -1)
        {
            std::cerr << "Failed to acquire swap chain image!" << std::endl;
            return;
        }

        resetCommandBuffer(cmdBuffer);

        auto framebuffer = recreateFramebuffer(swapChainImgIndex);
        record(cmdBuffer, framebuffer);
        submitToQueue(cmdBuffer);
        present(swapChainImgIndex);

        m_copyActions.clear();
        for (auto& item : m_renderPasses)
            item.second.second.clear();
        m_currentFrameIndex = (m_currentFrameIndex + 1) % NumVirtualFrames;

        destroyObjectsScheduledForRemoval();
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

    void VulkanRenderer::scheduleStagingBufferForRemoval(VkBuffer buffer)
    {
        if (m_oldStagingBuffers.find(buffer) == m_oldStagingBuffers.end())
        {
            m_oldStagingBuffers.emplace(buffer, NumVirtualFrames);
        }
    }

    void VulkanRenderer::scheduleStagingBufferForRemoval(VkBuffer buffer, MemoryChunk chunk)
    {
        if (m_oldStagingBuffersAndChunks.find(buffer) == m_oldStagingBuffersAndChunks.end())
        {
            m_oldStagingBuffersAndChunks.emplace(buffer, std::make_pair(chunk, NumVirtualFrames));
        }
    }

    void VulkanRenderer::destroyObjectsScheduledForRemoval()
    {
        if (!m_oldStagingBuffers.empty())
        {
            // Decrement remaining age of old staging buffers
            for (auto& el : m_oldStagingBuffers)
                el.second--;

            // Remove those with 0 frames remaining
            for (auto it = m_oldStagingBuffers.begin(), ite = m_oldStagingBuffers.end(); it != ite;)
            {
                if (it->second == 0) // time to die, no frames remaining
                {
                    auto& buffer = it->first;
                    m_device->destroyStagingBuffer(buffer);
                    it = m_oldStagingBuffers.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            for (auto& el : m_oldStagingBuffersAndChunks)
                el.second.second--;

            for (auto it = m_oldStagingBuffersAndChunks.begin(), ite = m_oldStagingBuffersAndChunks.end(); it != ite;)
            {
                if (it->second.second == 0) // time to die, no frames remaining
                {
                    auto& buffer = it->first;
                    it->second.first.memoryHeap->free(it->second.first); // free from heap

                    vkDestroyBuffer(m_device->getHandle(), buffer, nullptr);
                    it = m_oldStagingBuffersAndChunks.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    void VulkanRenderer::resize(int width, int height)
    {
        recreateSwapChain();
    }

    void VulkanRenderer::registerRenderPass(uint32_t key, VulkanRenderPass* renderPass)
    {
        m_renderPasses[key] = std::make_pair(renderPass, ActionVector());
    }

    void VulkanRenderer::unregisterRenderPass(uint32_t key)
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

    void VulkanRenderer::drawFullScreenQuad(VkCommandBuffer& cmdBuffer) const
    {
        m_fsQuadVertexBufferBindingGroup.bind(cmdBuffer);
        vkCmdBindIndexBuffer(cmdBuffer, m_fsQuadIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmdBuffer, 6, 1, 0, 0, 0);
    }
}