#include <Crisp/Renderer/Renderer.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/IO/FileUtils.hpp>
#include <Crisp/Renderer/IO/JsonPipelineBuilder.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/ShaderUtils/ShaderCompiler.hpp>

namespace crisp {
namespace {
auto logger = spdlog::stdout_color_mt("Renderer");

std::unique_ptr<VulkanRenderPass> createSwapChainRenderPass(
    const VulkanDevice& device, const VulkanSwapChain& swapChain, RenderTarget& renderTarget) {
    std::vector<RenderTarget*> renderTargets{&renderTarget};
    renderTargets[0]->info.buffered = true;
    renderTargets[0]->info.format = swapChain.getImageFormat();
    renderTargets[0]->info.sampleCount = VK_SAMPLE_COUNT_1_BIT;
    renderTargets[0]->info.isSwapChainDependent = true;
    renderTargets[0]->info.clearValue.color = {{0.0, 0.0f, 0.0f, 1.0f}};

    auto renderPass =
        RenderPassBuilder()
            .setAllocateAttachmentViews(false)

            .setAttachmentCount(1)
            .setAttachmentMapping(0, 0)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)

            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(
                VK_SUBPASS_EXTERNAL,
                0,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .create(device, swapChain.getExtent(), renderTargets);
    device.getDebugMarker().setObjectName(*renderPass, "Default Render Pass");
    return renderPass;
}

std::unique_ptr<Geometry> createFullScreenGeometry(Renderer& renderer) {
    const std::vector<glm::vec2> vertices = {{-1.0f, -1.0f}, {+3.0f, -1.0f}, {-1.0f, +3.0f}};
    const std::vector<glm::uvec3> faces = {{0, 2, 1}};
    return std::make_unique<Geometry>(renderer, vertices, faces);
}

} // namespace

Renderer::Renderer(
    std::vector<std::string>&& requiredVulkanInstanceExtensions,
    SurfaceCreator&& surfCreatorCallback,
    AssetPaths&& assetPaths,
    const bool enableRayTracingExtensions)
    : m_currentFrameIndex(0)
    , m_assetPaths(std::move(assetPaths))
    , m_defaultViewport()
    , m_defaultScissor() {
    recompileShaderDir(m_assetPaths.shaderSourceDir, m_assetPaths.spvShaderDir);

    std::vector<std::string> deviceExtensions = createDefaultDeviceExtensions();
    if (enableRayTracingExtensions) {
        addRayTracingDeviceExtensions(deviceExtensions);
    }

    // Create fundamental objects for the API
    m_context = std::make_unique<VulkanContext>(
        std::move(surfCreatorCallback), std::move(requiredVulkanInstanceExtensions), true);
    m_physicalDevice =
        std::make_unique<VulkanPhysicalDevice>(m_context->selectPhysicalDevice(std::move(deviceExtensions)).unwrap());
    m_device = std::make_unique<VulkanDevice>(
        *m_physicalDevice,
        createDefaultQueueConfiguration(*m_context, *m_physicalDevice),
        RendererConfig::VirtualFrameCount);
    m_swapChain = std::make_unique<VulkanSwapChain>(
        *m_device, *m_physicalDevice, m_context->getSurface(), TripleBuffering::Disabled);
    m_defaultRenderPass = createSwapChainRenderPass(*m_device, *m_swapChain, m_swapChainRenderTarget);

    m_defaultViewport = m_swapChain->getViewport();
    m_defaultScissor = m_swapChain->getScissorRect();

    // Create frame resources, such as command buffer, fence and semaphores
    m_virtualFrames.reserve(RendererConfig::VirtualFrameCount);
    for (int32_t i = 0; i < static_cast<int32_t>(RendererConfig::VirtualFrameCount); ++i) {
        m_virtualFrames.emplace_back(*m_device, i);
    }

    m_shaderCache = std::make_unique<ShaderCache>(m_device.get());

    m_workers.resize(1);
    for (auto& w : m_workers) {
        w = std::make_unique<VulkanWorker>(*m_device, m_device->getGeneralQueue(), NumVirtualFrames);
    }

    // Creates a map of all shaders
    // loadShaders(m_assetPaths.spvShaderDir);

    m_fullScreenGeometry = createFullScreenGeometry(*this);
    m_linearClampSampler = createLinearClampSampler(*m_device);
    m_scenePipeline = createPipelineFromJsonPath("GammaCorrect.json", *this, getDefaultRenderPass(), 0).unwrap();
    m_sceneMaterial = std::make_unique<Material>(m_scenePipeline.get());
}

Renderer::~Renderer() {} // NOLINT

const AssetPaths& Renderer::getAssetPaths() const {
    return m_assetPaths;
}

const std::filesystem::path& Renderer::getResourcesPath() const {
    return m_assetPaths.resourceDir;
}

std::filesystem::path Renderer::getShaderSourcePath(const std::string& shaderName) const {
    return m_assetPaths.shaderSourceDir / (shaderName + ".glsl");
}

VulkanContext& Renderer::getContext() const {
    return *m_context;
}

const VulkanPhysicalDevice& Renderer::getPhysicalDevice() const {
    return *m_physicalDevice;
}

VulkanDevice& Renderer::getDevice() const {
    return *m_device;
}

VulkanSwapChain& Renderer::getSwapChain() const {
    return *m_swapChain;
}

VkExtent2D Renderer::getSwapChainExtent() const {
    return m_swapChain->getExtent();
}

VkExtent3D Renderer::getSwapChainExtent3D() const {
    return {m_swapChain->getExtent().width, m_swapChain->getExtent().height, 1};
}

VulkanRenderPass& Renderer::getDefaultRenderPass() const {
    return *m_defaultRenderPass;
}

VkViewport Renderer::getDefaultViewport() const {
    return m_defaultViewport;
}

VkRect2D Renderer::getDefaultScissor() const {
    return m_defaultScissor;
}

VkShaderModule Renderer::getShaderModule(const std::string& key) const {
    return *m_shaderCache->getShaderModule(key);
}

VkShaderModule Renderer::getOrLoadShaderModule(const std::string& key) {
    return m_shaderCache->getOrLoadShaderModule(m_assetPaths.getSpvShaderPath(key));
}

void Renderer::setDefaultViewport(VkCommandBuffer cmdBuffer) const {
    vkCmdSetViewport(cmdBuffer, 0, 1, &m_defaultViewport);
}

void Renderer::setDefaultScissor(VkCommandBuffer cmdBuffer) const {
    vkCmdSetScissor(cmdBuffer, 0, 1, &m_defaultScissor);
}

void Renderer::drawFullScreenQuad(VkCommandBuffer cmdBuffer) const {
    m_fullScreenGeometry->bindAndDraw(cmdBuffer);
}

uint32_t Renderer::getCurrentVirtualFrameIndex() const {
    return m_currentFrameIndex % NumVirtualFrames;
}

uint64_t Renderer::getCurrentFrameIndex() const {
    return m_currentFrameIndex;
}

void Renderer::resize(int /*width*/, int /*height*/) {
    recreateSwapChain();

    flushResourceUpdates(true);
}

void Renderer::enqueueResourceUpdate(std::function<void(VkCommandBuffer)> resourceUpdate) {
    m_resourceUpdates.emplace_back(std::move(resourceUpdate));
}

void Renderer::enqueueDefaultPassDrawCommand(std::function<void(VkCommandBuffer)> drawAction) {
    m_defaultPassDrawCommands.emplace_back(std::move(drawAction));
}

void Renderer::enqueueDrawCommand(std::function<void(VkCommandBuffer)> drawAction) {
    m_drawCommands.emplace_back(std::move(drawAction));
}

void Renderer::flushResourceUpdates(bool waitOnAllQueues) {
    if (m_resourceUpdates.empty()) {
        return;
    }

    const VulkanQueue& generalQueue = m_device->getGeneralQueue();
    auto cmdPool = generalQueue.createCommandPool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = cmdPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    vkAllocateCommandBuffers(m_device->getHandle(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    for (const auto& update : m_resourceUpdates) {
        update(commandBuffer);
    }

    vkEndCommandBuffer(commandBuffer);

    VkFence tempFence = m_device->createFence(0);
    generalQueue.submit(commandBuffer, tempFence);
    m_device->wait(tempFence);

    if (waitOnAllQueues) {
        finish();
    }

    vkDestroyFence(m_device->getHandle(), tempFence, nullptr);

    vkFreeCommandBuffers(m_device->getHandle(), cmdPool, 1, &commandBuffer);
    vkResetCommandPool(m_device->getHandle(), cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
    vkDestroyCommandPool(m_device->getHandle(), cmdPool, nullptr);

    m_resourceUpdates.clear();
}

FrameContext Renderer::beginFrame() {
    const uint32_t virtualFrameIndex = getCurrentVirtualFrameIndex();
    // Obtain a frame that we can safely draw into
    auto& frame = m_virtualFrames[virtualFrameIndex];
    frame.waitCompletion(m_device->getHandle());

    std::function<void()> task;
    while (m_mainThreadQueue.tryPop(task)) {
        task();
    }

    // Destroy AFTER acquiring command buffer when NumVirtualFrames have passed
    m_device->getResourceDeallocator().decrementLifetimes();

    // Flush all noncoherent updates
    m_device->flushMappedRanges();

    const std::optional<uint32_t> swapImageIndex = acquireSwapImageIndex(frame);
    if (!swapImageIndex.has_value()) {
        logger->error("Failed to acquire swap chain image!");
        return {};
    }

    updateSwapChainRenderPass(virtualFrameIndex, m_swapChain->getImageView(*swapImageIndex));

    auto* commandBuffer = m_workers[0]->getCmdBuffer(virtualFrameIndex);
    commandBuffer->setIdleState();
    commandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    return {m_currentFrameIndex, virtualFrameIndex, *swapImageIndex, commandBuffer};
}

void Renderer::endFrame(const FrameContext& frameContext) {
    frameContext.commandBuffer->end();
    auto& frame = m_virtualFrames[frameContext.virtualFrameIndex];
    frame.addSubmission(*frameContext.commandBuffer);
    frameContext.commandBuffer->setExecutionState();

    frame.submitToQueue(m_device->getGeneralQueue());

    present(frame, frameContext.swapChainImageIndex);

    m_resourceUpdates.clear();
    m_drawCommands.clear();
    m_defaultPassDrawCommands.clear();

    ++m_currentFrameIndex;
}

void Renderer::drawFrame() {
    const auto frameCtx{beginFrame()};
    if (!frameCtx.commandBuffer) {
        return;
    }

    record(*frameCtx.commandBuffer);
    endFrame(frameCtx);
}

void Renderer::finish() {
    logger->warn("Calling vkDeviceWaitIdle()");
    vkDeviceWaitIdle(m_device->getHandle());
}

void Renderer::setSceneImageView(const VulkanRenderPass* renderPass, uint32_t renderTargetIndex) {
    if (renderPass) {
        m_sceneImageViews = renderPass->getAttachmentViews(renderTargetIndex);
        for (uint32_t i = 0; i < RendererConfig::VirtualFrameCount; ++i) {
            m_sceneMaterial->writeDescriptor(0, 0, i, *m_sceneImageViews[i], m_linearClampSampler.get());
        }

        m_device->flushDescriptorUpdates();
    } else // Prevent rendering any scene output to the screen
    {
        m_sceneImageViews.clear();
    }
}

void Renderer::setSceneImageViews(const std::vector<std::unique_ptr<VulkanImageView>>& imageViews) {
    m_sceneImageViews.clear();

    for (uint32_t i = 0; i < RendererConfig::VirtualFrameCount; ++i) {
        if (imageViews.size() == RendererConfig::VirtualFrameCount) {
            m_sceneImageViews.push_back(imageViews[i].get());
        } else {
            m_sceneImageViews.push_back(imageViews.front().get());
        }
    }

    m_sceneMaterial->writeDescriptor(
        0, 0, imageViews, m_linearClampSampler.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    m_device->flushDescriptorUpdates();
}

void Renderer::registerStreamingUniformBuffer(UniformBuffer* buffer) {
    m_streamingUniformBuffers.insert(buffer);
}

void Renderer::unregisterStreamingUniformBuffer(UniformBuffer* buffer) {
    m_streamingUniformBuffers.erase(buffer);
}

void Renderer::registerStreamingStorageBuffer(StorageBuffer* buffer) {
    m_streamingStorageBuffers.insert(buffer);
}

void Renderer::unregisterStreamingStorageBuffer(StorageBuffer* buffer) {
    m_streamingStorageBuffers.erase(buffer);
}

void Renderer::registerStreamingRingBuffer(VulkanRingBuffer* buffer) {
    m_streamingRingBuffers.insert(buffer);
}

void Renderer::unregisterStreamingRingBuffer(VulkanRingBuffer* buffer) {
    m_streamingRingBuffers.erase(buffer);
}

Geometry* Renderer::getFullScreenGeometry() const {
    return m_fullScreenGeometry.get();
}

std::unique_ptr<VulkanPipeline> Renderer::createPipeline(
    std::string_view pipelineName, const VulkanRenderPass& renderPass, uint32_t subpassIndex) {
    const std::filesystem::path absolutePipelinePath{getResourcesPath() / "Pipelines" / pipelineName};
    CRISP_CHECK(exists(absolutePipelinePath), "Path {} doesn't exist!", absolutePipelinePath.string());
    return createPipelineFromJsonPath(absolutePipelinePath, *this, renderPass, subpassIndex).unwrap();
}

void Renderer::updateInitialLayouts(VulkanRenderPass& renderPass) {
    enqueueResourceUpdate([&renderPass](VkCommandBuffer cmdBuffer) { renderPass.updateInitialLayouts(cmdBuffer); });
}

std::optional<uint32_t> Renderer::acquireSwapImageIndex(RendererFrame& frame) {
    uint32_t imageIndex{};
    const VkResult result = vkAcquireNextImageKHR(
        m_device->getHandle(),
        m_swapChain->getHandle(),
        std::numeric_limits<uint64_t>::max(),
        frame.getImageAvailableSemaphoreHandle(),
        VK_NULL_HANDLE,
        &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        logger->warn("Swap chain is 'out of date'");
        recreateSwapChain();
        return std::nullopt;
    }
    if (result == VK_SUBOPTIMAL_KHR) {
        logger->warn("Unable to acquire optimal VulkanSwapChain image!");
        return std::nullopt;
    }
    if (result != VK_SUCCESS) {
        logger->critical("Encountered an unknown error with vkAcquireNextImageKHR!");
        return std::nullopt;
    }

    return imageIndex;
}

void Renderer::record(const VulkanCommandBuffer& commandBuffer) {
    const uint32_t virtualFrameIndex = getCurrentVirtualFrameIndex();
    const VkCommandBuffer cmdBuffer{commandBuffer.getHandle()};

    for (auto& uniformBuffer : m_streamingUniformBuffers) {
        uniformBuffer->updateDeviceBuffer(cmdBuffer, virtualFrameIndex);
    }
    for (auto& storageBuffer : m_streamingStorageBuffers) {
        storageBuffer->updateDeviceBuffer(cmdBuffer, virtualFrameIndex);
    }
    for (auto& storageBuffer : m_streamingRingBuffers) {
        storageBuffer->updateDeviceBuffer(cmdBuffer, virtualFrameIndex);
    }

    commandBuffer.insertMemoryBarrier(
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_ACCESS_SHADER_READ_BIT);

    for (const auto& update : m_resourceUpdates) {
        update(cmdBuffer);
    }

    for (const auto& drawCommand : m_drawCommands) {
        drawCommand(cmdBuffer);
    }

    if (!m_sceneImageViews.empty()) {
        m_sceneImageViews[virtualFrameIndex]->getImage().transitionLayout(
            cmdBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }

    m_defaultRenderPass->begin(cmdBuffer, virtualFrameIndex, VK_SUBPASS_CONTENTS_INLINE);
    if (!m_sceneImageViews.empty()) {
        m_scenePipeline->bind(cmdBuffer);
        setDefaultViewport(cmdBuffer);
        setDefaultScissor(cmdBuffer);
        m_sceneMaterial->bind(virtualFrameIndex, cmdBuffer);
        drawFullScreenQuad(cmdBuffer);
    }

    for (const auto& drawCommand : m_defaultPassDrawCommands) {
        drawCommand(cmdBuffer);
    }
    m_defaultRenderPass->end(cmdBuffer, virtualFrameIndex);
}

void Renderer::present(RendererFrame& frame, uint32_t swapChainImageIndex) {
    const VkResult result = m_device->getGeneralQueue().present(
        frame.getRenderFinishedSemaphoreHandle(), m_swapChain->getHandle(), swapChainImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        CRISP_FATAL("Failed to present swap chain image!");
    }
}

void Renderer::recreateSwapChain() {
    vkDeviceWaitIdle(m_device->getHandle());

    m_swapChain->recreate(*m_device, *m_physicalDevice, m_context->getSurface());
    m_defaultScissor.extent = m_swapChain->getExtent();
    m_defaultViewport.width = static_cast<float>(m_defaultScissor.extent.width);
    m_defaultViewport.height = static_cast<float>(m_defaultScissor.extent.height);

    m_swapChainRenderTarget.info.size = m_swapChain->getExtent();
    m_defaultRenderPass->recreate(*m_device, m_swapChain->getExtent());
}

void Renderer::updateSwapChainRenderPass(uint32_t virtualFrameIndex, VkImageView swapChainImageView) {
    auto& framebuffer = m_defaultRenderPass->getFramebuffer(virtualFrameIndex);
    if (framebuffer && framebuffer->getAttachment(0) == swapChainImageView) {
        return;
    }

    if (!m_swapChainFramebuffers.contains(swapChainImageView)) {
        const auto attachmentViews = {swapChainImageView};
        m_swapChainFramebuffers.emplace(
            swapChainImageView,
            std::make_unique<VulkanFramebuffer>(
                *m_device, m_defaultRenderPass->getHandle(), m_swapChain->getExtent(), attachmentViews));
    }

    // Release ownership of the framebuffer into the pool of swap chain framebuffers.
    if (framebuffer) {
        framebuffer.swap(m_swapChainFramebuffers.at(framebuffer->getAttachment(0)));
    }

    // Acquire ownership of the framebuffer used for the current image to be rendered.
    framebuffer.swap(m_swapChainFramebuffers.at(swapChainImageView));
}

void fillDeviceBuffer(
    Renderer& renderer, VulkanBuffer* buffer, const void* data, const VkDeviceSize size, const VkDeviceSize offset) {
    auto stagingBuffer = std::make_shared<StagingVulkanBuffer>(renderer.getDevice(), size);
    stagingBuffer->updateFromHost(data);
    renderer.enqueueResourceUpdate([stagingBuffer, buffer, offset, size](VkCommandBuffer cmdBuffer) {
        buffer->copyFrom(cmdBuffer, *stagingBuffer, 0, offset, size);
    });
}

} // namespace crisp