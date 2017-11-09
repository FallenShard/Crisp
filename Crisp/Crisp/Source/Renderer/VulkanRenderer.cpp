#include "VulkanRenderer.hpp"

#define NOMINMAX

#include <iostream>

#include "Math/Headers.hpp"
#include "IO/FileUtils.hpp"

#include "Vulkan/VulkanQueue.hpp"
#include "Vulkan/VulkanDevice.hpp"
#include "Vulkan/VulkanSwapChain.hpp"
#include "Vulkan/VulkanBuffer.hpp"
#include "vulkan/VulkanImage.hpp"

#include "Renderer/Texture.hpp"
#include "Renderer/TextureView.hpp"
#include "Renderer/VertexBuffer.hpp"
#include "Renderer/IndexBuffer.hpp"
#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"

namespace crisp
{
    namespace
    {
        static constexpr auto shadersDir = "Resources/Shaders/";
    }

    VulkanRenderer::VulkanRenderer(SurfaceCreator surfCreatorCallback, std::vector<std::string>&& extensions)
        : m_framesRendered(0)
        , m_currentFrameIndex(0)
    {
        // Create fundamental objects for the API
        m_context           = std::make_unique<VulkanContext>(surfCreatorCallback, std::forward<std::vector<std::string>>(extensions));
        m_device            = std::make_unique<VulkanDevice>(m_context.get());
        m_swapChain         = std::make_unique<VulkanSwapChain>(m_device.get(), NumVirtualFrames);
        m_defaultRenderPass = std::make_unique<DefaultRenderPass>(this);

        m_defaultViewport = { 0.0f, 0.0f, static_cast<float>(m_swapChain->getExtent().width), static_cast<float>(m_swapChain->getExtent().height), 0.0f, 1.0f };

        // Create frame resources, such as command buffer, fence and semaphores
        for (auto& frameRes : m_frameResources)
        {
            frameRes.cmdPool = m_device->getGeneralQueue()->createCommandPoolFromFamily(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

            VkCommandBufferAllocateInfo cmdBufallocInfo = {};
            cmdBufallocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cmdBufallocInfo.commandPool        = frameRes.cmdPool;
            cmdBufallocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cmdBufallocInfo.commandBufferCount = 1;
            vkAllocateCommandBuffers(m_device->getHandle(), &cmdBufallocInfo, &frameRes.cmdBuffer);

            frameRes.bufferFinishedFence     = m_device->createFence(VK_FENCE_CREATE_SIGNALED_BIT);
            frameRes.imageAvailableSemaphore = m_device->createSemaphore();
            frameRes.renderFinishedSemaphore = m_device->createSemaphore();
        }

        // Creates a map of all shaders
        loadShaders(shadersDir);

        // create vertex buffer
        std::vector<glm::vec2> vertices =
        {
            { -1.0f, -1.0f },
            { +1.0f, -1.0f },
            { +1.0f, +1.0f },
            { -1.0f, +1.0f }
        };
        m_fsQuadVertexBuffer = std::make_unique<VertexBuffer>(this, vertices);
        m_fsQuadVertexBufferBindingGroup =
        {
            { m_fsQuadVertexBuffer->get(), 0 }
        };

        // create index buffer
        std::vector<glm::u16vec3> faces =
        {
            { 0, 2, 1 },
            { 0, 3, 2 }
        };
        m_fsQuadIndexBuffer = std::make_unique<IndexBuffer>(this, faces);
    }

    VulkanRenderer::~VulkanRenderer()
    {
        for (auto& frameRes : m_frameResources)
        {
            vkDestroyCommandPool(m_device->getHandle(), frameRes.cmdPool, nullptr);
            vkDestroyFence(m_device->getHandle(), frameRes.bufferFinishedFence, nullptr);
            vkDestroySemaphore(m_device->getHandle(), frameRes.imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(m_device->getHandle(), frameRes.renderFinishedSemaphore, nullptr);
        }

        for (auto& shaderModule : m_shaderModules)
        {
            vkDestroyShaderModule(m_device->getHandle(), shaderModule.second, nullptr);
        }
    }

    VulkanContext* VulkanRenderer::getContext() const
    {
        return m_context.get();
    }

    VulkanDevice* VulkanRenderer::getDevice() const
    {
        return m_device.get();
    }

    VulkanSwapChain* VulkanRenderer::getSwapChain() const
    {
        return m_swapChain.get();
    }

    VkExtent2D VulkanRenderer::getSwapChainExtent() const
    {
        return m_swapChain->getExtent();
    }

    DefaultRenderPass* VulkanRenderer::getDefaultRenderPass() const
    {
        return m_defaultRenderPass.get();
    }

    VkViewport VulkanRenderer::getDefaultViewport() const
    {
        return m_defaultViewport;
    }

    VkShaderModule VulkanRenderer::getShaderModule(std::string&& key) const
    {
        return m_shaderModules.at(key);
    }

    void VulkanRenderer::setDefaultViewport(VkCommandBuffer cmdBuffer) const
    {
        vkCmdSetViewport(cmdBuffer, 0, 1, &m_defaultViewport);
    }

    void VulkanRenderer::drawFullScreenQuad(VkCommandBuffer cmdBuffer) const
    {
        m_fsQuadVertexBufferBindingGroup.bind(cmdBuffer);
        m_fsQuadIndexBuffer->bind(cmdBuffer, 0);
        vkCmdDrawIndexed(cmdBuffer, 6, 1, 0, 0, 0);
    }

    uint32_t VulkanRenderer::getCurrentVirtualFrameIndex() const
    {
        return m_currentFrameIndex;
    }

    void VulkanRenderer::resize(int width, int height)
    {
        recreateSwapChain();

        m_defaultViewport.width  = static_cast<float>(m_swapChain->getExtent().width);
        m_defaultViewport.height = static_cast<float>(m_swapChain->getExtent().height);

        for (auto& pipeline : m_pipelines)
            pipeline->resize(width, height);

        flushResourceUpdates();
    }

    void VulkanRenderer::registerPipeline(VulkanPipeline* pipeline)
    {
        m_pipelines.insert(pipeline);
    }

    void VulkanRenderer::unregisterPipeline(VulkanPipeline* pipeline)
    {
        m_pipelines.erase(pipeline);
    }

    void VulkanRenderer::enqueueResourceUpdate(std::function<void(VkCommandBuffer)> resourceUpdate)
    {
        m_resourceUpdates.emplace_back(resourceUpdate);
    }

    void VulkanRenderer::enqueueDefaultPassDrawCommand(std::function<void(VkCommandBuffer)> drawAction)
    {
        m_defaultPassDrawCommands.emplace_back(drawAction);
    }

    void VulkanRenderer::enqueueDrawCommand(std::function<void(VkCommandBuffer)> drawAction)
    {
        m_drawCommands.emplace_back(drawAction);
    }

    void VulkanRenderer::flushResourceUpdates()
    {
        if (m_resourceUpdates.empty())
            return;

        auto generalQueue = m_device->getGeneralQueue();
        auto cmdPool = generalQueue->createCommandPoolFromFamily(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool        = cmdPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_device->getHandle(), &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        for (const auto& update : m_resourceUpdates)
            update(commandBuffer);

        vkEndCommandBuffer(commandBuffer);

        generalQueue->submit(commandBuffer);
        generalQueue->waitIdle();

        vkFreeCommandBuffers(m_device->getHandle(), cmdPool, 1, &commandBuffer);
        vkDestroyCommandPool(m_device->getHandle(), cmdPool, nullptr);

        m_resourceUpdates.clear();
    }

    void VulkanRenderer::drawFrame()
    {
        auto cmdBuffer = acquireCommandBuffer();
        
        // Destroy AFTER acquiring command buffer when NumVirtualFrames have passed
        destroyResourcesScheduledForRemoval();
        
        m_device->flushMappedRanges();

        std::optional<uint32_t> swapChainImageIndex = acquireSwapChainImageIndex();
        if (!swapChainImageIndex.has_value())
        {
            std::cerr << "Failed to acquire swap chain image!" << std::endl;
            return;
        }

        m_defaultRenderPass->recreateFramebuffer(m_swapChain->getImageView(swapChainImageIndex.value()));
        record(cmdBuffer);
        const auto& frameRes = m_frameResources[m_currentFrameIndex];
        m_device->getGeneralQueue()->submit(
            frameRes.imageAvailableSemaphore,
            frameRes.renderFinishedSemaphore,
            frameRes.cmdBuffer,
            frameRes.bufferFinishedFence
        );
        
        present(swapChainImageIndex.value());

        m_resourceUpdates.clear();
        m_drawCommands.clear();
        m_defaultPassDrawCommands.clear();

        m_currentFrameIndex = (m_currentFrameIndex + 1) % NumVirtualFrames;
    }

    void VulkanRenderer::finish()
    {
        vkDeviceWaitIdle(m_device->getHandle());
    }

    void VulkanRenderer::fillDeviceBuffer(VulkanBuffer* buffer, const void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        auto stagingBuffer = std::make_shared<VulkanBuffer>(m_device.get(), size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        stagingBuffer->updateFromHost(data, size, 0);
        m_resourceUpdates.emplace_back([this, stagingBuffer, buffer, offset, size](VkCommandBuffer cmdBuffer)
        {
            buffer->copyFrom(cmdBuffer, *stagingBuffer, 0, offset, size);

            scheduleBufferForRemoval(stagingBuffer);
        });
    }

    void VulkanRenderer::scheduleBufferForRemoval(std::shared_ptr<VulkanBuffer> buffer, uint32_t framesToLive)
    {
        if (m_removedBuffers.find(buffer) == m_removedBuffers.end())
            m_removedBuffers.emplace(buffer, framesToLive);
    }

    void VulkanRenderer::destroyResourcesScheduledForRemoval()
    {
        if (!m_removedBuffers.empty())
        {
            // Decrement remaining age of removed buffers
            for (auto& el : m_removedBuffers)
                el.second--;

            // Remove those with 0 frames remaining
            for (auto iter = m_removedBuffers.begin(), end = m_removedBuffers.end(); iter != end;)
            {
                if (iter->second <= 0) // time to die, no frames remaining
                    iter = m_removedBuffers.erase(iter); // Free memory and destroy buffer in destructor
                else
                    ++iter;
            }
        }
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
        vkResetFences(m_device->getHandle(),   1, &m_frameResources[m_currentFrameIndex].bufferFinishedFence);
        return m_frameResources[m_currentFrameIndex].cmdBuffer;
    }

    std::optional<uint32_t> VulkanRenderer::acquireSwapChainImageIndex()
    {
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(m_device->getHandle(), m_swapChain->getHandle(), std::numeric_limits<uint64_t>::max(), m_frameResources[m_currentFrameIndex].imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapChain();
            return {};
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            std::cerr << "Unable to acquire optimal swapchain image!\n";
            return {};
        }

        return imageIndex;
    }

    void VulkanRenderer::resetCommandBuffer(VkCommandBuffer cmdBuffer)
    {
        vkResetCommandBuffer(cmdBuffer, 0);
    }

    void VulkanRenderer::record(VkCommandBuffer commandBuffer)
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        for (const auto& update : m_resourceUpdates)
            update(commandBuffer);

        for (const auto& drawCommand : m_drawCommands)
            drawCommand(commandBuffer);

        m_defaultRenderPass->begin(commandBuffer);
        for (const auto& drawCommand : m_defaultPassDrawCommands)
            drawCommand(commandBuffer);
        m_defaultRenderPass->end(commandBuffer);
        
        vkEndCommandBuffer(commandBuffer);
    }

    void VulkanRenderer::present(uint32_t swapChainImageIndex)
    {
        auto result = m_device->getGeneralQueue()->present(
            m_frameResources[m_currentFrameIndex].renderFinishedSemaphore,
            m_swapChain->getHandle(), swapChainImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS)
        {
            throw std::exception("Failed to present swap chain image!");
        }
    }

    void VulkanRenderer::recreateSwapChain()
    {
        vkDeviceWaitIdle(m_device->getHandle());

        m_swapChain->recreate();
        m_defaultRenderPass->recreate();
    }
}