#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <Crisp/Core/ThreadPool.hpp>
#include <Crisp/Renderer/AssetPaths.hpp>
#include <Crisp/Renderer/FrameContext.hpp>
#include <Crisp/Renderer/RenderTargetCache.hpp>
#include <Crisp/Renderer/RendererConfig.hpp>
#include <Crisp/Renderer/RendererFrame.hpp>
#include <Crisp/Renderer/ShaderCache.hpp>
#include <Crisp/Renderer/VulkanWorker.hpp>
#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/Rhi/VulkanInstance.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSwapChain.hpp>
#include <Crisp/Vulkan/VulkanDebugUtils.hpp>
#include <Crisp/Vulkan/VulkanRingBuffer.hpp>

namespace crisp {
class Geometry;
class Material;

class Renderer {
public:
    static constexpr uint32_t NumVirtualFrames = kRendererVirtualFrameCount;

    Renderer(
        std::vector<std::string>&& requiredVulkanInstanceExtensions,
        SurfaceCreator&& surfCreatorCallback,
        AssetPaths&& assetPaths,
        bool enableRayTracingExtensions);
    ~Renderer(); // Defined in .cpp due to symbol definition visibility.

    Renderer(const Renderer& other) = delete;
    Renderer(Renderer&& other) = delete;
    Renderer& operator=(const Renderer& other) = delete;
    Renderer& operator=(Renderer&& other) = delete;

    const AssetPaths& getAssetPaths() const;
    const std::filesystem::path& getResourcesPath() const;
    std::filesystem::path getShaderSourcePath(const std::string& shaderName) const;

    VulkanInstance& getInstance() const;
    const VulkanPhysicalDevice& getPhysicalDevice() const;
    VulkanDevice& getDevice() const;
    VulkanSwapChain& getSwapChain() const;
    VkExtent2D getSwapChainExtent() const;
    VkExtent3D getSwapChainExtent3D() const;

    VulkanRenderPass& getDefaultRenderPass() const;
    VkViewport getDefaultViewport() const;
    VkRect2D getDefaultScissor() const;

    VkShaderModule getShaderModule(const std::string& key) const;
    VkShaderModule getOrLoadShaderModule(const std::string& key);

    void setDefaultViewport(VkCommandBuffer cmdBuffer) const;
    void setDefaultScissor(VkCommandBuffer cmdBuffer) const;
    void drawFullScreenQuad(VkCommandBuffer cmdBuffer) const;

    uint32_t getCurrentVirtualFrameIndex() const;
    uint64_t getCurrentFrameIndex() const;

    void resize(int width, int height);

    void enqueueResourceUpdate(const std::function<void(VkCommandBuffer)>& resourceUpdate);
    void enqueueDrawCommand(std::function<void(VkCommandBuffer)> drawAction);
    void enqueueDefaultPassDrawCommand(std::function<void(VkCommandBuffer)> drawAction);

    void flushResourceUpdates(bool waitOnAllQueues);

    FrameContext beginFrame();
    void endFrame(const FrameContext& frameContext);
    void drawFrame();

    void finish();

    void setSceneImageView(const VulkanRenderPass* renderPass, uint32_t renderTargetIndex);
    void setSceneImageViews(const std::vector<std::unique_ptr<VulkanImageView>>& imageViews);

    void registerStreamingRingBuffer(VulkanRingBuffer* buffer);
    void unregisterStreamingRingBuffer(VulkanRingBuffer* buffer);

    Geometry* getFullScreenGeometry() const;

    std::unique_ptr<VulkanPipeline> createPipeline(
        std::string_view pipelineName, const VulkanRenderPass& renderPass, uint32_t subpassIndex);

    void updateInitialLayouts(VulkanRenderPass& renderPass);

    void schedule(std::function<void()>&& task) {
        m_threadPool.schedule(std::move(task));
    }

    void scheduleOnMainThread(std::function<void()>&& task) {
        m_mainThreadQueue.push(std::move(task));
    }

    const VulkanDebugMarker& getDebugMarker() const {
        return m_device->getDebugMarker();
    }

private:
    std::optional<uint32_t> acquireSwapImageIndex(RendererFrame& virtualFrame);
    void record(const VulkanCommandBuffer& commandBuffer);
    void present(RendererFrame& virtualFrame, uint32_t swapChainImageIndex);

    void recreateSwapChain();
    void updateSwapChainRenderPass(uint32_t virtualFrameIndex, VkImageView swapChainImageView);

    uint64_t m_currentFrameIndex;
    AssetPaths m_assetPaths;

    std::unique_ptr<VulkanInstance> m_instance;
    std::unique_ptr<VulkanPhysicalDevice> m_physicalDevice;
    std::unique_ptr<VulkanDevice> m_device;
    std::unique_ptr<VulkanSwapChain> m_swapChain;
    std::unique_ptr<VulkanRenderPass> m_defaultRenderPass;

    FlatHashMap<VkImageView, std::unique_ptr<VulkanFramebuffer>> m_swapChainFramebuffers;

    VkViewport m_defaultViewport;
    VkRect2D m_defaultScissor;

    std::vector<RendererFrame> m_virtualFrames;

    std::unique_ptr<ShaderCache> m_shaderCache;

    using FunctionVector = std::vector<std::function<void(VkCommandBuffer)>>;
    FunctionVector m_drawCommands;
    FunctionVector m_defaultPassDrawCommands;

    std::unique_ptr<Geometry> m_fullScreenGeometry;

    FlatHashSet<VulkanRingBuffer*> m_streamingRingBuffers;

    std::unique_ptr<VulkanPipeline> m_scenePipeline;
    std::unique_ptr<VulkanSampler> m_linearClampSampler;
    std::unique_ptr<Material> m_sceneMaterial;

    std::vector<VulkanImageView*> m_sceneImageViews;

    std::vector<std::unique_ptr<VulkanWorker>> m_workers;

    RenderTarget m_swapChainRenderTarget;

    ThreadPool m_threadPool;
    ConcurrentQueue<std::function<void()>> m_mainThreadQueue;
};

void fillDeviceBuffer(
    Renderer& renderer, VulkanBuffer* buffer, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

template <typename T>
inline void fillDeviceBuffer(
    Renderer& renderer, VulkanBuffer* buffer, const std::vector<T>& data, VkDeviceSize offset = 0) {
    fillDeviceBuffer(renderer, buffer, data.data(), data.size() * sizeof(T), offset);
}

} // namespace crisp