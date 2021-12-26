#include <Crisp/Renderer/Renderer.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>

#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanQueue.hpp>
#include <Crisp/Vulkan/VulkanSwapChain.hpp>

#include <Crisp/Vulkan/VulkanBuffer.hpp>
#include <Crisp/Vulkan/VulkanCommandBuffer.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanSampler.hpp>
#include <Crisp/vulkan/VulkanCommandPool.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Renderer/LuaPipelineBuilder.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/RenderPasses/DefaultRenderPass.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>
#include <Crisp/ShadingLanguage/ShaderCompiler.hpp>

#include <CrispCore/ChromeProfiler.hpp>
#include <CrispCore/IO/FileUtils.hpp>
#include <CrispCore/Math/Headers.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace crisp
{
namespace
{
auto logger = spdlog::stdout_color_mt("Renderer");
}

Renderer::Renderer(SurfaceCreator surfCreatorCallback)
    : m_currentFrameIndex(0)
    , m_resourcesPath(ApplicationEnvironment::getResourcesPath())
{
    recompileShaderDir(ApplicationEnvironment::getShaderSourcesPath(), m_resourcesPath / "Shaders");

    // Create fundamental objects for the API
    std::vector<std::string> deviceExtensions = createDefaultDeviceExtensions();
    // deviceExtensions.push_back(VK_NV_RAY_TRACING_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    deviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);

    m_context = std::make_unique<VulkanContext>(surfCreatorCallback,
        ApplicationEnvironment::getRequiredVulkanExtensions(), true);
    m_physicalDevice =
        std::make_unique<VulkanPhysicalDevice>(m_context->selectPhysicalDevice(std::move(deviceExtensions)).unwrap());
    m_device = std::make_unique<VulkanDevice>(*m_physicalDevice,
        createDefaultQueueConfiguration(*m_context, *m_physicalDevice), RendererConfig::VirtualFrameCount);
    m_swapChain = std::make_unique<VulkanSwapChain>(*m_device, *m_context, false);
    m_defaultRenderPass = std::make_unique<DefaultRenderPass>(*this);
    logger->info("Created all base components");

    m_defaultViewport = m_swapChain->getViewport();
    m_defaultScissor = m_swapChain->getScissorRect();

    // Create frame resources, such as command buffer, fence and semaphores
    for (auto& frame : m_virtualFrames)
    {
        frame.completionFence = m_device->createFence(0);
        frame.imageAvailableSemaphore = m_device->createSemaphore();
        frame.renderFinishedSemaphore = m_device->createSemaphore();
    }

    m_workers.resize(1);
    for (auto& w : m_workers)
        w = std::make_unique<VulkanWorker>(*m_device, m_device->getGeneralQueue(), NumVirtualFrames);

    // Creates a map of all shaders
    loadShaders(m_resourcesPath / "Shaders");

    // create vertex buffer
    const std::vector<glm::vec2> vertices = {
        {-1.0f, -1.0f},
        {+3.0f, -1.0f},
        {-1.0f, +3.0f}
    };
    const std::vector<glm::uvec3> faces = {
        {0, 2, 1}
    };
    m_fullScreenGeometry = std::make_unique<Geometry>(this, vertices, faces);

    m_linearClampSampler = std::make_unique<VulkanSampler>(*m_device, VK_FILTER_LINEAR, VK_FILTER_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    m_scenePipeline = createPipelineFromLua("Tonemapping.lua", getDefaultRenderPass(), 0);
    m_sceneMaterial = std::make_unique<Material>(m_scenePipeline.get());
}

Renderer::~Renderer()
{
    for (auto& frameRes : m_virtualFrames)
    {
        vkDestroyFence(m_device->getHandle(), frameRes.completionFence, nullptr);
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
    return ApplicationEnvironment::getShaderSourcesPath() / (shaderName + ".glsl");
}

VulkanContext& Renderer::getContext() const
{
    return *m_context;
}

const VulkanPhysicalDevice& Renderer::getPhysicalDevice() const
{
    return *m_physicalDevice;
}

VulkanDevice& Renderer::getDevice() const
{
    return *m_device;
}

VulkanSwapChain& Renderer::getSwapChain() const
{
    return *m_swapChain;
}

VkExtent2D Renderer::getSwapChainExtent() const
{
    return m_swapChain->getExtent();
}

VkExtent3D Renderer::getSwapChainExtent3D() const
{
    return { m_swapChain->getExtent().width, m_swapChain->getExtent().height, 1 };
}

DefaultRenderPass& Renderer::getDefaultRenderPass() const
{
    return *m_defaultRenderPass;
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
    logger->info("Compiling: {}", path.string());
    std::string command = "glslangValidator.exe -V -o ";
    command += (m_resourcesPath / "Shaders" / (id + ".spv")).make_preferred().string();
    command += ' ';
    command += (ApplicationEnvironment::getShaderSourcesPath() / path).make_preferred().string();
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
    return m_currentFrameIndex % NumVirtualFrames;
}

uint64_t Renderer::getCurrentFrameIndex() const
{
    return m_currentFrameIndex;
}

void Renderer::resize(int /*width*/, int /*height*/)
{
    recreateSwapChain();

    m_defaultViewport.width = static_cast<float>(m_swapChain->getExtent().width);
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

    const VulkanQueue& generalQueue = m_device->getGeneralQueue();
    auto cmdPool = generalQueue.createCommandPool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = cmdPool;
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
    generalQueue.submit(commandBuffer, tempFence);
    m_device->wait(tempFence);

    if (waitOnAllQueues)
        finish();

    vkDestroyFence(m_device->getHandle(), tempFence, nullptr);

    vkFreeCommandBuffers(m_device->getHandle(), cmdPool, 1, &commandBuffer);
    vkResetCommandPool(m_device->getHandle(), cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
    vkDestroyCommandPool(m_device->getHandle(), cmdPool, nullptr);

    m_resourceUpdates.clear();
}

void Renderer::drawFrame()
{
    const uint32_t virtualFrameIndex = getCurrentVirtualFrameIndex();
    // Obtain a frame that we can safely draw into
    auto& frame = m_virtualFrames[virtualFrameIndex];
    frame.waitCompletion(m_device->getHandle());

    // Destroy AFTER acquiring command buffer when NumVirtualFrames have passed
    m_device->getResourceDeallocator().incrementFrameCount();

    // Flush all noncoherent updates
    m_device->flushMappedRanges();

    std::optional<uint32_t> swapImageIndex = acquireSwapImageIndex(frame);
    if (!swapImageIndex.has_value())
    {
        logger->error("Failed to acquire swap chain image!");
        return;
    }

    m_defaultRenderPass->recreateFramebuffer(*this, m_swapChain->getImageView(swapImageIndex.value()));

    /*for (const auto& worker : m_workers)
    {*/
    auto* cmdBuffer = m_workers[0]->getCmdBuffer(virtualFrameIndex);
    cmdBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    record(cmdBuffer->getHandle());
    cmdBuffer->end();
    frame.addSubmission(*cmdBuffer);
    //}

    frame.submitToQueue(m_device->getGeneralQueue());

    present(frame, swapImageIndex.value());

    m_resourceUpdates.clear();
    m_drawCommands.clear();
    m_defaultPassDrawCommands.clear();

    ++m_currentFrameIndex;
}

void Renderer::finish()
{
    logger->warn("Calling vkDeviceWaitIdle()");
    vkDeviceWaitIdle(m_device->getHandle());
}

void Renderer::fillDeviceBuffer(VulkanBuffer* buffer, const void* data, VkDeviceSize size, VkDeviceSize offset)
{
    auto stagingBuffer = std::make_shared<StagingVulkanBuffer>(*m_device, size);
    stagingBuffer->updateFromHost(data);
    m_resourceUpdates.emplace_back(
        [this, stagingBuffer, buffer, offset, size](VkCommandBuffer cmdBuffer)
        {
            buffer->copyFrom(cmdBuffer, *stagingBuffer, 0, offset, size);
        });
}

void Renderer::setSceneImageView(const VulkanRenderPass* renderPass, uint32_t renderTargetIndex)
{
    if (renderPass)
    {
        m_sceneImageViews = renderPass->getRenderTargetViews(renderTargetIndex);
        for (uint32_t i = 0; i < RendererConfig::VirtualFrameCount; ++i)
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
    for (uint32_t i = 0; i < RendererConfig::VirtualFrameCount; ++i)
        m_sceneImageViews.push_back(imageViews[i].get());

    m_sceneMaterial->writeDescriptor(0, 0, imageViews, m_linearClampSampler.get(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

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

std::unique_ptr<VulkanPipeline> Renderer::createPipelineFromLua(std::string_view pipelineName,
    const VulkanRenderPass& renderPass, int subpassIndex)
{
    return LuaPipelineBuilder(getResourcesPath() / "Pipelines" / pipelineName).create(this, renderPass, subpassIndex);
}

void Renderer::loadShaders(const std::filesystem::path& directoryPath)
{
    auto files = enumerateFiles(directoryPath, "spv");
    for (auto& file : files)
        loadSpirvShaderModule(directoryPath / file);

    logger->info("Loaded all {} shaders", files.size());
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
            logger->info("Reloading shader module: {}", shaderKey);
            vkDestroyShaderModule(m_device->getHandle(), existingItem->second.handle, nullptr);
        }
    }

    const auto shaderCode = readBinaryFile(shaderModulePath).unwrap();

    VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

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
        logger->warn("Swap chain is 'out of date'");
        recreateSwapChain();
        return {};
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        logger->warn("Unable to acquire optimal VulkanSwapChain image!");
        return {};
    }

    return imageIndex;
}

void Renderer::record(VkCommandBuffer commandBuffer)
{
    const uint32_t virtualFrameIndex = getCurrentVirtualFrameIndex();

    for (auto& uniformBuffer : m_streamingUniformBuffers)
        uniformBuffer->updateDeviceBuffer(commandBuffer, virtualFrameIndex);
    for (const auto& update : m_resourceUpdates)
        update(commandBuffer);
    for (const auto& drawCommand : m_drawCommands)
        drawCommand(commandBuffer);

    m_defaultRenderPass->begin(commandBuffer, virtualFrameIndex, VK_SUBPASS_CONTENTS_INLINE);
    if (!m_sceneImageViews.empty())
    {
        m_scenePipeline->bind(commandBuffer);
        setDefaultViewport(commandBuffer);
        setDefaultScissor(commandBuffer);
        m_sceneMaterial->bind(virtualFrameIndex, commandBuffer);
        drawFullScreenQuad(commandBuffer);
    }

    for (const auto& drawCommand : m_defaultPassDrawCommands)
        drawCommand(commandBuffer);
    m_defaultRenderPass->end(commandBuffer, virtualFrameIndex);
}

void Renderer::present(VirtualFrame& virtualFrame, uint32_t swapChainImageIndex)
{
    const VkResult result = m_device->getGeneralQueue().present(virtualFrame.renderFinishedSemaphore,
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

    m_swapChain->recreate(*m_device, *m_context);
    m_defaultRenderPass->recreate(*this);
}
} // namespace crisp