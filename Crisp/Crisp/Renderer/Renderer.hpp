#pragma once

#include <Crisp/Core/ThreadPool.hpp>

#include <Crisp/Renderer/RendererConfig.hpp>
#include <Crisp/Renderer/VirtualFrame.hpp>
#include <Crisp/Renderer/VulkanWorker.hpp>

#include <Crisp/Renderer/RenderTargetCache.hpp>
#include <Crisp/Vulkan/VulkanContext.hpp>

#include <array>
#include <coroutine>
#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

namespace crisp
{
class VulkanDevice;
class VulkanSwapChain;
class VulkanFramebuffer;
class VulkanRenderPass;
class VulkanPipeline;
class VulkanSampler;
class VulkanImageView;
class VulkanBuffer;
class VulkanCommandPool;
class VulkanCommandBuffer;

class UniformBuffer;
class Geometry;
class Material;

class Renderer
{
public:
    static constexpr uint32_t NumVirtualFrames = RendererConfig::VirtualFrameCount;

    Renderer(SurfaceCreator surfCreatorCallback);
    ~Renderer();

    Renderer(const Renderer& other) = delete;
    Renderer(Renderer&& other) = delete;
    Renderer& operator=(const Renderer& other) = delete;
    Renderer& operator=(Renderer&& other) = delete;

    const std::filesystem::path& getResourcesPath() const;
    std::filesystem::path getShaderSourcePath(const std::string& shaderName) const;

    VulkanContext& getContext() const;
    const VulkanPhysicalDevice& getPhysicalDevice() const;
    VulkanDevice& getDevice() const;
    VulkanSwapChain& getSwapChain() const;
    VkExtent2D getSwapChainExtent() const;
    VkExtent3D getSwapChainExtent3D() const;

    VulkanRenderPass& getDefaultRenderPass() const;
    VkViewport getDefaultViewport() const;
    VkRect2D getDefaultScissor() const;

    VkShaderModule loadShaderModule(const std::string& key);
    VkShaderModule compileGlsl(const std::filesystem::path& path, std::string id, std::string type);
    VkShaderModule getShaderModule(const std::string& key) const;

    void setDefaultViewport(VkCommandBuffer cmdBuffer) const;
    void setDefaultScissor(VkCommandBuffer cmdBuffer) const;
    void drawFullScreenQuad(VkCommandBuffer cmdBuffer) const;

    uint32_t getCurrentVirtualFrameIndex() const;
    uint64_t getCurrentFrameIndex() const;

    void resize(int width, int height);

    void enqueueResourceUpdate(std::function<void(VkCommandBuffer)> resourceUpdate);
    void enqueueDrawCommand(std::function<void(VkCommandBuffer)> drawAction);
    void enqueueDefaultPassDrawCommand(std::function<void(VkCommandBuffer)> drawAction);

    void flushResourceUpdates(bool waitOnAllQueues);
    void flushCoroutines();

    void drawFrame();

    void finish();

    void fillDeviceBuffer(VulkanBuffer* buffer, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    template <typename T>
    inline void fillDeviceBuffer(VulkanBuffer* buffer, const std::vector<T>& data, VkDeviceSize offset = 0)
    {
        fillDeviceBuffer(buffer, data.data(), data.size() * sizeof(T), offset);
    }

    void setSceneImageView(const VulkanRenderPass* renderPass, uint32_t renderTargetIndex);
    void setSceneImageViews(const std::vector<std::unique_ptr<VulkanImageView>>& imageViews);

    void registerStreamingUniformBuffer(UniformBuffer* buffer);
    void unregisterStreamingUniformBuffer(UniformBuffer* buffer);

    Geometry* getFullScreenGeometry() const;

    std::unique_ptr<VulkanPipeline> createPipelineFromLua(
        std::string_view pipelineName, const VulkanRenderPass& renderPass, int subpassIndex);

    template <typename... Args>
    std::unique_ptr<UniformBuffer> createUniformBuffer(Args&&... args)
    {
        return std::make_unique<UniformBuffer>(this, std::forward<Args>(args)...);
    }

    auto getNextCommandBuffer()
    {
        struct Awaitable
        {
            Renderer* renderer{nullptr};

            bool await_ready() const noexcept
            {
                return false;
            }

            void await_suspend(std::coroutine_handle<> h) noexcept
            {
                renderer->m_cmdBufferCoroutines.push_back(h);
            }

            VkCommandBuffer await_resume() const noexcept
            {
                return renderer->m_coroCmdBuffer;
            }
        };

        return Awaitable{this};
    }

    void updateInitialLayouts(VulkanRenderPass& renderPass);

    void schedule(std::function<void()>&& task)
    {
        m_threadPool.schedule(std::move(task));
    }

    void scheduleOnMainThread(std::function<void()>&& task)
    {
        m_mainThreadQueue.push(std::move(task));
    }

private:
    void loadShaders(const std::filesystem::path& directoryPath);
    VkShaderModule loadSpirvShaderModule(const std::filesystem::path& shaderModulePath);
    std::optional<uint32_t> acquireSwapImageIndex(VirtualFrame& virtualFrame);
    void record(VkCommandBuffer commandBuffer);
    void present(VirtualFrame& virtualFrame, uint32_t swapChainImageIndex);

    void recreateSwapChain();
    void updateSwapChainRenderPass(uint32_t virtualFrameIndex, VkImageView swapChainImageView);

    uint64_t m_currentFrameIndex;
    std::filesystem::path m_assetDirectory;

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<VulkanPhysicalDevice> m_physicalDevice;
    std::unique_ptr<VulkanDevice> m_device;
    std::unique_ptr<VulkanSwapChain> m_swapChain;
    std::unique_ptr<VulkanRenderPass> m_defaultRenderPass;
    robin_hood::unordered_flat_map<VkImageView, std::unique_ptr<VulkanFramebuffer>> m_swapChainFramebuffers;

    VkViewport m_defaultViewport;
    VkRect2D m_defaultScissor;

    std::array<VirtualFrame, NumVirtualFrames> m_virtualFrames;

    struct ShaderModule
    {
        VkShaderModule handle;
        std::filesystem::file_time_type lastModifiedTimestamp;
    };

    std::unordered_map<std::string, ShaderModule> m_shaderModules;

    using FunctionVector = std::vector<std::function<void(VkCommandBuffer)>>;
    FunctionVector m_resourceUpdates;
    FunctionVector m_drawCommands;
    FunctionVector m_defaultPassDrawCommands;

    std::unique_ptr<Geometry> m_fullScreenGeometry;

    std::unordered_set<UniformBuffer*> m_streamingUniformBuffers;

    std::unique_ptr<VulkanPipeline> m_scenePipeline;
    std::unique_ptr<VulkanSampler> m_linearClampSampler;
    std::unique_ptr<Material> m_sceneMaterial;

    std::vector<VulkanImageView*> m_sceneImageViews;

    std::vector<std::unique_ptr<VulkanWorker>> m_workers;

    std::list<std::coroutine_handle<>> m_cmdBufferCoroutines;
    VkCommandBuffer m_coroCmdBuffer;

    RenderTarget m_swapChainRenderTarget;

    ThreadPool m_threadPool;
    ConcurrentQueue<std::function<void()>> m_mainThreadQueue;
};
} // namespace crisp