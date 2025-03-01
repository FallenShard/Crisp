#include <Crisp/Renderer/Renderer.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/ChromeEventTracer.hpp>
#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Io/FileUtils.hpp>
#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Renderer/VulkanPipelineIo.hpp>
#include <Crisp/ShaderUtils/ShaderCompiler.hpp>

namespace crisp {
namespace {
auto logger = spdlog::stdout_color_mt("Renderer");

std::unique_ptr<VulkanRenderPass> createSwapChainRenderPass(
    const VulkanDevice& device, const VulkanSwapChain& swapChain) {

    RenderPassCreationParams creationParams{};
    creationParams.clearValues.push_back({.color = {{0.0, 0.0f, 0.0f, 1.0f}}});

    auto renderPass =
        RenderPassBuilder()
            .setAttachmentCount(1)
            .setAttachmentFormat(0, swapChain.getImageFormat())
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)

            .setSubpassCount(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, kExternalColorSubpass >> kColorWrite)
            .create(device, swapChain.getExtent(), creationParams);
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
    VulkanCoreParams&& vulkanCoreParams, SurfaceCreator&& surfCreatorCallback, AssetPaths&& assetPaths) // NOLINT
    : m_currentFrameIndex(0)
    , m_assetPaths(std::move(assetPaths))
    , m_defaultViewport()
    , m_defaultScissor() {
    recompileShaderDir(m_assetPaths.shaderSourceDir, m_assetPaths.spvShaderDir);

    // Create fundamental objects for the API
    m_instance = std::make_unique<VulkanInstance>(
        std::move(surfCreatorCallback), std::move(vulkanCoreParams.requiredInstanceExtensions), true);
    m_physicalDevice = std::make_unique<VulkanPhysicalDevice>(
        selectPhysicalDevice(*m_instance, std::move(vulkanCoreParams.requiredDeviceExtensions)).unwrap());
    m_device = std::make_unique<VulkanDevice>(
        *m_physicalDevice,
        createDefaultQueueConfiguration(*m_instance, *m_physicalDevice),
        *m_instance,
        kRendererVirtualFrameCount);
    m_swapChain = std::make_unique<VulkanSwapChain>(
        *m_device, *m_physicalDevice, m_instance->getSurface(), vulkanCoreParams.presentationMode);
    m_defaultRenderPass = createSwapChainRenderPass(*m_device, *m_swapChain);

    m_defaultViewport = m_swapChain->getViewport();
    m_defaultScissor = m_swapChain->getScissorRect();

    // Create frame resources, such as command buffer, fence and semaphores.
    m_virtualFrames.reserve(kRendererVirtualFrameCount);
    for (int32_t i = 0; i < static_cast<int32_t>(kRendererVirtualFrameCount); ++i) {
        m_virtualFrames.emplace_back(*m_device, i);
    }

    m_shaderCache = std::make_unique<ShaderCache>(m_device.get());

    m_workers.resize(1);
    for (auto& w : m_workers) {
        w = std::make_unique<VulkanWorker>(*m_device, m_device->getGeneralQueue(), NumVirtualFrames);
    }

    m_fullScreenGeometry = createFullScreenGeometry(*this);
    m_linearClampSampler = createLinearClampSampler(*m_device);
    m_scenePipeline = createPipeline("GammaCorrect.json", getDefaultRenderPass());
    m_sceneMaterial = std::make_unique<Material>(m_scenePipeline.get());
}

Renderer::~Renderer() {} // NOLINT

const AssetPaths& Renderer::getAssetPaths() const {
    return m_assetPaths;
}

const std::filesystem::path& Renderer::getResourcesPath() const {
    return m_assetPaths.resourceDir;
}

VulkanInstance& Renderer::getInstance() const {
    return *m_instance;
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
    return m_shaderCache->getOrLoadShaderModule(m_assetPaths.getShaderSpvPath(key));
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

void Renderer::enqueueResourceUpdate(const std::function<void(VkCommandBuffer)>& resourceUpdate) {
    m_device->postResourceUpdate(resourceUpdate);
}

void Renderer::enqueueDefaultPassDrawCommand(std::function<void(VkCommandBuffer)> drawAction) {
    m_defaultPassDrawCommands.emplace_back(std::move(drawAction));
}

void Renderer::enqueueDrawCommand(std::function<void(VkCommandBuffer)> drawAction) {
    m_drawCommands.emplace_back(std::move(drawAction));
}

void Renderer::flushResourceUpdates(bool waitOnAllQueues) {
    m_device->flushResourceUpdates(waitOnAllQueues);
}

std::optional<FrameContext> Renderer::beginFrame() {
    const uint32_t virtualFrameIndex = getCurrentVirtualFrameIndex();
    // Obtain a frame that we can safely draw into
    auto& frame = m_virtualFrames[virtualFrameIndex];
    {
        CRISP_TRACE_SCOPE("frame_wait");
        frame.waitCompletion(*m_device);
    }

    std::function<void()> task;
    while (m_mainThreadQueue.tryPop(task)) {
        task();
    }

    // Destroy AFTER acquiring command buffer when NumVirtualFrames have passed
    m_device->getResourceDeallocator().decrementLifetimes();

    // Flush all noncoherent updates
    m_device->flushMappedRanges();
    m_device->flushDescriptorUpdates();

    const std::optional<uint32_t> swapChainImageIndex = acquireSwapImageIndex(frame);
    if (!swapChainImageIndex.has_value()) {
        CRISP_LOGE("Failed to acquire swap chain image!");
        return std::nullopt;
    }

    const VkImageView swapChainImageView = m_swapChain->getImageView(*swapChainImageIndex);
    if (!m_swapChainFramebuffers.contains(swapChainImageView)) {
        const auto attachmentViews = {swapChainImageView};
        m_swapChainFramebuffers.emplace(
            swapChainImageView,
            std::make_unique<VulkanFramebuffer>(
                *m_device, m_defaultRenderPass->getHandle(), m_swapChain->getExtent(), attachmentViews));
    }

    auto* commandBuffer = m_workers[0]->resetAndGetCmdBuffer(*m_device, virtualFrameIndex);
    commandBuffer->setIdleState();
    commandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    return FrameContext{
        .frameIndex = m_currentFrameIndex,
        .virtualFrameIndex = virtualFrameIndex,
        .swapChainImageIndex = *swapChainImageIndex,
        .commandBuffer = commandBuffer,
        .commandEncoder = VulkanCommandEncoder(commandBuffer->getHandle()),
    };
}

void Renderer::record(const FrameContext& frameContext) {
    const auto& encoder{frameContext.commandEncoder};
    const auto cmdBuffer = encoder.getHandle();

    encoder.insertBarrier(kTransferWrite >> (kComputeRead | kVertexRead | kFragmentRead));

    m_device->executeResourceUpdates(cmdBuffer);

    for (const auto& drawCommand : m_drawCommands) {
        drawCommand(cmdBuffer);
    }

    if (m_sceneImageView) {
        encoder.transitionLayout(
            m_sceneImageView->getImage(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kColorWrite >> kFragmentSampledRead);
    }

    const auto& swapChainImageView{m_swapChain->getImageView(frameContext.swapChainImageIndex)};
    const auto& framebuffer{*m_swapChainFramebuffers.at(swapChainImageView)};

    encoder.beginRenderPass(*m_defaultRenderPass, framebuffer);
    if (m_sceneImageView) {
        encoder.bindPipeline(*m_scenePipeline);
        encoder.setViewport(m_defaultViewport);
        encoder.setScissor(m_defaultScissor);
        m_sceneMaterial->bind(cmdBuffer);
        drawFullScreenQuad(cmdBuffer);
    }

    for (const auto& drawCommand : m_defaultPassDrawCommands) {
        drawCommand(cmdBuffer);
    }
    m_defaultRenderPass->end(cmdBuffer);
}

void Renderer::endFrame(const FrameContext& frameContext) {
    frameContext.commandBuffer->end();
    auto& frame = m_virtualFrames[frameContext.virtualFrameIndex];
    frame.addSubmission(*frameContext.commandBuffer);
    frameContext.commandBuffer->setExecutionState();

    frame.submitToQueue(m_device->getGeneralQueue());

    present(frame, frameContext.swapChainImageIndex);

    m_drawCommands.clear();
    m_defaultPassDrawCommands.clear();

    ++m_currentFrameIndex;
}

void Renderer::finish() {
    CRISP_LOGW("Calling vkDeviceWaitIdle()");
    m_device->waitIdle();
}

void Renderer::setSceneImageView(const VulkanImageView* imageView) {
    m_sceneImageView = imageView;
    if (m_sceneImageView) {
        m_sceneMaterial->writeDescriptor(0, 0, m_sceneImageView->getDescriptorInfo(m_linearClampSampler.get()));
        m_device->flushDescriptorUpdates();
    }
}

Geometry* Renderer::getFullScreenGeometry() const {
    return m_fullScreenGeometry.get();
}

std::unique_ptr<VulkanPipeline> Renderer::createPipeline(
    const std::string_view pipelineName, const VulkanRenderPass& renderPass, const uint32_t subpassIndex) {
    const std::filesystem::path absolutePipelinePath{getResourcesPath() / "Pipelines" / pipelineName};
    CRISP_CHECK(exists(absolutePipelinePath), "Path {} doesn't exist!", absolutePipelinePath.string());
    return createPipelineFromFile(
               absolutePipelinePath, m_assetPaths.spvShaderDir, *m_shaderCache, *m_device, renderPass, subpassIndex)
        .unwrap();
}

void Renderer::updateInitialLayouts(VulkanRenderPass& renderPass) {
    // enqueueResourceUpdate([&renderPass](VkCommandBuffer cmdBuffer) { renderPass.updateInitialLayouts(cmdBuffer); });
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
        CRISP_LOGW("Swap chain is 'out of date'");
        recreateSwapChain();
        return std::nullopt;
    }
    if (result == VK_SUBOPTIMAL_KHR) {
        CRISP_LOGW("Unable to acquire optimal VulkanSwapChain image!");
        return std::nullopt;
    }
    if (result != VK_SUCCESS) {
        CRISP_LOGF("Encountered an unknown error with vkAcquireNextImageKHR!");
        return std::nullopt;
    }

    return imageIndex;
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

    m_swapChain->recreate(*m_device, *m_physicalDevice, m_instance->getSurface());
    m_defaultScissor.extent = m_swapChain->getExtent();
    m_defaultViewport.width = static_cast<float>(m_defaultScissor.extent.width);
    m_defaultViewport.height = static_cast<float>(m_defaultScissor.extent.height);

    m_defaultRenderPass->setRenderArea(m_swapChain->getExtent());
}

void fillDeviceBuffer(
    Renderer& renderer, VulkanBuffer* buffer, const void* data, const VkDeviceSize size, const VkDeviceSize offset) {
    auto stagingBuffer = std::make_shared<StagingVulkanBuffer>(renderer.getDevice(), size);
    stagingBuffer->updateFromHost(data);
    renderer.getDevice().getGeneralQueue().submitAndWait(
        [stagingBuffer, buffer, offset, size](VkCommandBuffer cmdBuffer) {
            buffer->copyFrom(cmdBuffer, *stagingBuffer, 0, offset, size);
        });
}

} // namespace crisp