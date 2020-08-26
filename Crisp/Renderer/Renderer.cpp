#include "Renderer.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Vulkan/VulkanSwapChain.hpp"
#include "Vulkan/VulkanQueue.hpp"

#include "Renderer/Pipelines/FullScreenQuadPipeline.hpp"
#include "Vulkan/VulkanSampler.hpp"
#include "Vulkan/VulkanImageView.hpp"
#include "Vulkan/VulkanBuffer.hpp"
#include "vulkan/VulkanCommandPool.hpp"
#include "Vulkan/VulkanCommandBuffer.hpp"

#include "Renderer/RenderPasses/DefaultRenderPass.hpp"

#include "Renderer/UniformBuffer.hpp"
#include "Geometry/Geometry.hpp"
#include "Renderer/Material.hpp"

#include "IO/FileUtils.hpp"

#include <CrispCore/Log.hpp>
#include <CrispCore/Math/Headers.hpp>

#include "LuaPipelineBuilder.hpp"

namespace crisp
{
    namespace
    {
        static const std::filesystem::path ShaderSourcePath("D:/version-control/Crisp/Crisp/Shaders");
    }

    Renderer::Renderer(SurfaceCreator surfCreatorCallback, std::vector<std::string>&& extensions, std::filesystem::path&& resourcesPath)
        : m_numFramesRendered(0)
        , m_currentFrameIndex(0)
        , m_resourcesPath(std::move(resourcesPath))
    {
        std::string command = (m_resourcesPath / "CrispShaderCompiler.exe").make_preferred().string();
        command += ' ';
        command += std::filesystem::path(ShaderSourcePath).make_preferred().string();
        command += ' ';
        command += (m_resourcesPath / "Shaders").make_preferred().string();
        std::system(command.c_str());

        // Create fundamental objects for the API
        m_context           = std::make_unique<VulkanContext>(surfCreatorCallback, std::forward<std::vector<std::string>>(extensions));
        m_device            = std::make_unique<VulkanDevice>(m_context.get(), NumVirtualFrames);
        m_swapChain         = std::make_unique<VulkanSwapChain>(m_device.get(), true);
        m_defaultRenderPass = std::make_unique<DefaultRenderPass>(this);

        m_defaultViewport = m_swapChain->createViewport();
        m_defaultScissor = m_swapChain->createScissor();

        // Create frame resources, such as command buffer, fence and semaphores
        for (auto& frame : m_virtualFrames)
        {
            frame.cmdPool   = std::make_unique<VulkanCommandPool>(m_device->getGeneralQueue(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
            frame.cmdBuffer = std::make_unique<VulkanCommandBuffer>(frame.cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
            frame.bufferFinishedFence     = m_device->createFence(VK_FENCE_CREATE_SIGNALED_BIT);
            frame.imageAvailableSemaphore = m_device->createSemaphore();
            frame.renderFinishedSemaphore = m_device->createSemaphore();
        }

        // Creates a map of all shaders
        loadShaders(m_resourcesPath / "Shaders");

        // create vertex buffer
        std::vector<glm::vec2> vertices = { { -1.0f, -1.0f }, { +3.0f, -1.0f }, { -1.0f, +3.0f } };
        std::vector<glm::uvec3> faces = { { 0, 2, 1 } };
        m_fullScreenGeometry = std::make_unique<Geometry>(this, vertices, faces);

        m_linearClampSampler = std::make_unique<VulkanSampler>(m_device.get(), VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        m_scenePipeline = createTonemappingPipeline(this, getDefaultRenderPass(), 0, true);
        m_sceneMaterial = std::make_unique<Material>(m_scenePipeline.get());
    }

    Renderer::~Renderer()
    {
        for (auto& frameRes : m_virtualFrames)
        {
            vkDestroyFence(m_device->getHandle(), frameRes.bufferFinishedFence, nullptr);
            vkDestroySemaphore(m_device->getHandle(), frameRes.imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(m_device->getHandle(), frameRes.renderFinishedSemaphore, nullptr);
        }

        for (auto& shaderModule : m_shaderModules)
        {
            vkDestroyShaderModule(m_device->getHandle(), shaderModule.second.handle, nullptr);
        }
    }

    const std::filesystem::path& Renderer::getResourcesPath() const
    {
        return m_resourcesPath;
    }

    std::filesystem::path Renderer::getShaderSourcePath(const std::string& shaderName) const
    {
        return ShaderSourcePath / (shaderName + ".glsl");
    }

    VulkanContext* Renderer::getContext() const
    {
        return m_context.get();
    }

    VulkanDevice* Renderer::getDevice() const
    {
        return m_device.get();
    }

    VulkanSwapChain* Renderer::getSwapChain() const
    {
        return m_swapChain.get();
    }

    VkExtent2D Renderer::getSwapChainExtent() const
    {
        return m_swapChain->getExtent();
    }

    DefaultRenderPass* Renderer::getDefaultRenderPass() const
    {
        return m_defaultRenderPass.get();
    }

    VkViewport Renderer::getDefaultViewport() const
    {
        return m_defaultViewport;
    }

    VkRect2D Renderer::getDefaultScissor() const
    {
        return m_defaultScissor;
    }

    VkShaderModule Renderer::loadShaderModule(const std::string& key)
    {
        return loadSpirvShaderModule(m_resourcesPath / "Shaders" / (key + ".spv"));
    }

    VkShaderModule Renderer::compileGlsl(const std::filesystem::path& path, std::string id, std::string type)
    {
        std::cout << "Compiling: " << path << '\n';
        std::string command = "glslangValidator.exe -V -o ";
        command += (m_resourcesPath / "Shaders" / (id + ".spv")).make_preferred().string();
        command += ' ';
        command += (ShaderSourcePath / path).make_preferred().string();
        std::system(command.c_str());

        return loadShaderModule(id);
    }

    VkShaderModule Renderer::getShaderModule(const std::string& key) const
    {
        return m_shaderModules.at(key).handle;
    }

    void Renderer::setDefaultViewport(VkCommandBuffer cmdBuffer) const
    {
        vkCmdSetViewport(cmdBuffer, 0, 1, &m_defaultViewport);
    }

    void Renderer::setDefaultScissor(VkCommandBuffer cmdBuffer) const
    {
        vkCmdSetScissor(cmdBuffer, 0, 1, &m_defaultScissor);
    }

    void Renderer::drawFullScreenQuad(VkCommandBuffer cmdBuffer) const
    {
        m_fullScreenGeometry->bindAndDraw(cmdBuffer);
    }

    uint32_t Renderer::getCurrentVirtualFrameIndex() const
    {
        return m_currentFrameIndex;
    }

    void Renderer::resize(int /*width*/, int /*height*/)
    {
        recreateSwapChain();

        m_defaultViewport.width  = static_cast<float>(m_swapChain->getExtent().width);
        m_defaultViewport.height = static_cast<float>(m_swapChain->getExtent().height);
        m_defaultScissor.extent = m_swapChain->getExtent();

        flushResourceUpdates(true);
    }

    void Renderer::enqueueResourceUpdate(std::function<void(VkCommandBuffer)> resourceUpdate)
    {
        m_resourceUpdates.emplace_back(resourceUpdate);
    }

    void Renderer::enqueueDefaultPassDrawCommand(std::function<void(VkCommandBuffer)> drawAction)
    {
        m_defaultPassDrawCommands.emplace_back(drawAction);
    }

    void Renderer::enqueueDrawCommand(std::function<void(VkCommandBuffer)> drawAction)
    {
        m_drawCommands.emplace_back(drawAction);
    }

    void Renderer::flushResourceUpdates(bool waitOnAllQueues)
    {
        if (m_resourceUpdates.empty())
            return;

        auto generalQueue = m_device->getGeneralQueue();
        auto cmdPool = generalQueue->createCommandPool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool        = cmdPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_device->getHandle(), &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        for (const auto& update : m_resourceUpdates)
            update(commandBuffer);

        vkEndCommandBuffer(commandBuffer);

        VkFence tempFence = m_device->createFence(0);
        generalQueue->submit(commandBuffer, tempFence);
        generalQueue->wait(tempFence);
        vkDestroyFence(m_device->getHandle(), tempFence, nullptr);

        if (waitOnAllQueues)
            finish();

        vkFreeCommandBuffers(m_device->getHandle(), cmdPool, 1, &commandBuffer);
        vkResetCommandPool(m_device->getHandle(), cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
        vkDestroyCommandPool(m_device->getHandle(), cmdPool, nullptr);

        m_resourceUpdates.clear();
    }

    void Renderer::drawFrame()
    {
        auto& frame = m_virtualFrames[m_currentFrameIndex];

        frame.waitCommandBuffer(m_device->getHandle());

        // Destroy AFTER acquiring command buffer when NumVirtualFrames have passed
        destroyResourcesScheduledForRemoval();

        m_device->updateDeferredDestructions();
        m_device->flushMappedRanges();

        std::optional<uint32_t> swapImageIndex = acquireSwapImageIndex(frame);
        if (!swapImageIndex.has_value())
        {
            logError("Failed to acquire swap chain image!\n");
            return;
        }

        m_defaultRenderPass->recreateFramebuffer(m_swapChain->getImageView(swapImageIndex.value()));

        frame.cmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        record(frame.cmdBuffer->getHandle());
        frame.cmdBuffer->end();
        frame.submit(m_device->getGeneralQueue());
        present(frame, swapImageIndex.value());

        m_resourceUpdates.clear();
        m_drawCommands.clear();
        m_defaultPassDrawCommands.clear();

        m_currentFrameIndex = (m_currentFrameIndex + 1) % NumVirtualFrames;
    }

    void Renderer::finish()
    {
        logTagWarning("Renderer", "Calling vkDeviceWaitIdle()\n");
        vkDeviceWaitIdle(m_device->getHandle());
    }

    void Renderer::fillDeviceBuffer(VulkanBuffer* buffer, const void* data, VkDeviceSize size, VkDeviceSize offset)
    {
        auto stagingBuffer = std::make_shared<VulkanBuffer>(m_device.get(), size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        stagingBuffer->updateFromHost(data);
        m_resourceUpdates.emplace_back([this, stagingBuffer, buffer, offset, size](VkCommandBuffer cmdBuffer)
        {
            buffer->copyFrom(cmdBuffer, *stagingBuffer, 0, offset, size);

            scheduleBufferForRemoval(stagingBuffer);
        });
    }

    void Renderer::scheduleBufferForRemoval(std::shared_ptr<VulkanBuffer> buffer, uint32_t framesToLive)
    {
        if (m_removedBuffers.find(buffer) == m_removedBuffers.end())
            m_removedBuffers.emplace(buffer, framesToLive);
    }

    void Renderer::setSceneImageView(const VulkanRenderPass* renderPass, uint32_t renderTargetIndex)
    {
        if (renderPass)
        {
            m_sceneImageViews = renderPass->getRenderTargetViews(renderTargetIndex);
            for (uint32_t i = 0; i < Renderer::NumVirtualFrames; ++i)
                m_sceneMaterial->writeDescriptor(0, 0, i, *m_sceneImageViews[i], m_linearClampSampler.get());

            m_device->flushDescriptorUpdates();
        }
        else // Prevent rendering any scene output to the screen
        {
            m_sceneImageViews.clear();
        }
    }

    void Renderer::setSceneImageViews(const std::vector<std::unique_ptr<VulkanImageView>>& imageViews)
    {
        m_sceneImageViews.clear();
        for (uint32_t i = 0; i < Renderer::NumVirtualFrames; ++i)
            m_sceneImageViews.push_back(imageViews[i].get());

        m_sceneMaterial->writeDescriptor(0, 0, imageViews, m_linearClampSampler.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        m_device->flushDescriptorUpdates();
    }

    void Renderer::registerStreamingUniformBuffer(UniformBuffer* buffer)
    {
        m_streamingUniformBuffers.insert(buffer);
    }

    void Renderer::unregisterStreamingUniformBuffer(UniformBuffer* buffer)
    {
        m_streamingUniformBuffers.erase(buffer);
    }

    Geometry* Renderer::getFullScreenGeometry() const
    {
        return m_fullScreenGeometry.get();
    }

    std::unique_ptr<VulkanPipeline> Renderer::createPipelineFromLua(std::string_view pipelineName, const VulkanRenderPass& renderPass, int subpassIndex)
    {
        return LuaPipelineBuilder(getResourcesPath() / "Pipelines" / pipelineName).create(this, renderPass, subpassIndex);
    }

    void Renderer::destroyResourcesScheduledForRemoval()
    {
        if (!m_removedBuffers.empty())
        {
            // Decrement remaining age of removed buffers
            for (auto& keyValue : m_removedBuffers)
                keyValue.second--;

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

    void Renderer::loadShaders(const std::filesystem::path& directoryPath)
    {
        auto files = fileutils::enumerateFiles(directoryPath, "spv");
        for (auto& file : files)
            loadSpirvShaderModule(directoryPath / file);
    }

    VkShaderModule Renderer::loadSpirvShaderModule(const std::filesystem::path& shaderModulePath)
    {
        auto timestamp = std::filesystem::last_write_time(shaderModulePath);

        auto shaderKey = shaderModulePath.stem().string();
        auto existingItem = m_shaderModules.find(shaderKey);
        if (existingItem != m_shaderModules.end())
        {
            // Cached shader module is same or newer
            if (existingItem->second.lastModifiedTimestamp >= timestamp)
            {
                return existingItem->second.handle;
            }
            else
            {
                logInfo("Reloading shader module: {}\n", shaderKey);
                vkDestroyShaderModule(m_device->getHandle(), existingItem->second.handle, nullptr);
            }
        }

        auto shaderCode = fileutils::readBinaryFile(shaderModulePath);

        VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        createInfo.codeSize = shaderCode.size();
        createInfo.pCode    = reinterpret_cast<const uint32_t*>(shaderCode.data());

        VkShaderModule shaderModule(VK_NULL_HANDLE);
        vkCreateShaderModule(m_device->getHandle(), &createInfo, nullptr, &shaderModule);

        m_shaderModules.insert_or_assign(shaderKey, ShaderModule{ shaderModule, timestamp });
        return shaderModule;
    }

    std::optional<uint32_t> Renderer::acquireSwapImageIndex(VirtualFrame& virtualFrame)
    {
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(m_device->getHandle(), m_swapChain->getHandle(),
            std::numeric_limits<uint64_t>::max(), virtualFrame.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapChain();
            return {};
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            logError("Unable to acquire optimal VulkanSwapChain image!");
            return {};
        }

        return imageIndex;
    }

    void Renderer::resetCommandBuffer(VkCommandBuffer cmdBuffer)
    {
        vkResetCommandBuffer(cmdBuffer, 0);
    }

    void Renderer::record(VkCommandBuffer commandBuffer)
    {
        for (auto& uniformBuffer : m_streamingUniformBuffers)
            uniformBuffer->updateDeviceBuffer(commandBuffer, m_currentFrameIndex);

        for (const auto& update : m_resourceUpdates)
            update(commandBuffer);

        for (const auto& drawCommand : m_drawCommands)
            drawCommand(commandBuffer);

        m_defaultRenderPass->begin(commandBuffer);
        if (!m_sceneImageViews.empty())
        {
            m_scenePipeline->bind(commandBuffer);
            setDefaultViewport(commandBuffer);
            setDefaultScissor(commandBuffer);
            m_sceneMaterial->bind(m_currentFrameIndex, commandBuffer);
            drawFullScreenQuad(commandBuffer);
        }

        for (const auto& drawCommand : m_defaultPassDrawCommands)
            drawCommand(commandBuffer);
        m_defaultRenderPass->end(commandBuffer, m_currentFrameIndex);
    }

    void Renderer::present(VirtualFrame& virtualFrame, uint32_t swapChainImageIndex)
    {
        VkResult result = m_device->getGeneralQueue()->present(virtualFrame.renderFinishedSemaphore,
            m_swapChain->getHandle(), swapChainImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to present swap chain image!");
        }
    }

    void Renderer::recreateSwapChain()
    {
        vkDeviceWaitIdle(m_device->getHandle());

        m_swapChain->recreate();
        m_defaultRenderPass->recreate();
    }
}