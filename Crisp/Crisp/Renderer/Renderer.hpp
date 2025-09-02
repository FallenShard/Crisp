#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <Crisp/Core/ThreadPool.hpp>
#include <Crisp/Renderer/AssetPaths.hpp>
#include <Crisp/Renderer/FrameContext.hpp>
#include <Crisp/Renderer/Material.hpp>
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
#include <Crisp/Vulkan/VulkanRingBuffer.hpp>
#include <Crisp/Vulkan/VulkanTracer.hpp>

namespace crisp {
class Geometry;

struct VulkanCoreParams {
    std::vector<std::string> requiredInstanceExtensions;
    std::vector<VulkanDeviceFeatureRequest> deviceFeatureRequests;
    PresentationMode presentationMode;
    bool includeValidation{true};
};

class Renderer {
public:
    static constexpr uint32_t NumVirtualFrames = kRendererVirtualFrameCount;

    Renderer(VulkanCoreParams&& vulkanCoreParams, SurfaceCreator&& surfCreatorCallback, AssetPaths&& assetPaths);
    ~Renderer(); // Defined in .cpp due to symbol definition visibility.

    Renderer(const Renderer& other) = delete;
    Renderer(Renderer&& other) = delete;
    Renderer& operator=(const Renderer& other) = delete;
    Renderer& operator=(Renderer&& other) = delete;

    const AssetPaths& getAssetPaths() const;
    const std::filesystem::path& getResourcesPath() const;

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

    std::optional<FrameContext> beginFrame();
    void record(const FrameContext& frameContext);
    void endFrame(const FrameContext& frameContext);

    void finish();

    void setSceneImageView(const VulkanImageView* imageView);

    Geometry* getFullScreenGeometry() const;

    std::unique_ptr<VulkanPipeline> createPipeline(
        std::string_view pipelineName, const VulkanRenderPass& renderPass, uint32_t subpassIndex = 0);

    void schedule(std::function<void()>&& task) {
        m_threadPool.schedule(std::move(task));
    }

    void scheduleOnMainThread(std::function<void()>&& task) {
        m_mainThreadQueue.push(std::move(task));
    }

    const ShaderCache& getShaderCache() const {
        return *m_shaderCache;
    }

    ShaderCache& getShaderCache() {
        return *m_shaderCache;
    }

private:
    std::optional<uint32_t> acquireSwapImageIndex(RendererFrame& virtualFrame);
    void present(RendererFrame& virtualFrame, uint32_t swapChainImageIndex);

    void recreateSwapChain();

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

    std::unique_ptr<VulkanPipeline> m_scenePipeline;
    std::unique_ptr<VulkanSampler> m_linearClampSampler;
    std::unique_ptr<Material> m_sceneMaterial;

    const VulkanImageView* m_sceneImageView{nullptr};

    std::vector<std::unique_ptr<VulkanWorker>> m_workers;

    ThreadPool m_threadPool;
    ConcurrentQueue<std::function<void()>> m_mainThreadQueue;

    std::vector<std::unique_ptr<VulkanTracingContext>> m_gpuTracingContexts;
};

void fillDeviceBuffer(
    Renderer& renderer, VulkanBuffer* buffer, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

template <typename T>
inline void fillDeviceBuffer(
    Renderer& renderer, VulkanBuffer* buffer, const std::vector<T>& data, VkDeviceSize offset = 0) {
    fillDeviceBuffer(renderer, buffer, data.data(), data.size() * sizeof(T), offset);
}

} // namespace crisp