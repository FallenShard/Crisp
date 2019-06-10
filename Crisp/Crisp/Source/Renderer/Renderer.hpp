#pragma once

#include "Vulkan/VulkanContext.hpp"

#include <memory>
#include <array>
#include <vector>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>

namespace crisp
{
    class VulkanDevice;
    class VulkanSwapChain;

    class VulkanRenderPass;
    class DefaultRenderPass;
    class VulkanPipeline;
    class VulkanSampler;
    class VulkanImageView;
    class VulkanBuffer;

    class UniformBuffer;
    class Geometry;
    class Material;

    class Renderer
    {
    public:
        static constexpr unsigned int NumVirtualFrames = 2;

        Renderer(SurfaceCreator surfCreatorCallback, std::vector<std::string>&& extensions, std::filesystem::path&& resourcesPath);
        ~Renderer();

        Renderer(const Renderer& other) = delete;
        Renderer(Renderer&& other) = delete;
        Renderer& operator=(const Renderer& other) = delete;
        Renderer& operator=(Renderer&& other) = delete;

        const std::filesystem::path& getResourcesPath() const;

        VulkanContext*    getContext() const;
        VulkanDevice*     getDevice() const;
        VulkanSwapChain*  getSwapChain() const;
        VkExtent2D        getSwapChainExtent() const;

        DefaultRenderPass* getDefaultRenderPass() const;
        VkViewport         getDefaultViewport() const;
        VkRect2D           getDefaultScissor() const;

        VkShaderModule    getShaderModule(std::string&& key) const;

        void setDefaultViewport(VkCommandBuffer cmdBuffer) const;
        void setDefaultScissor(VkCommandBuffer cmdBuffer) const;
        void drawFullScreenQuad(VkCommandBuffer cmdBuffer) const;

        uint32_t getCurrentVirtualFrameIndex() const;

        void resize(int width, int height);

        void enqueueResourceUpdate(std::function<void(VkCommandBuffer)> resourceUpdate);
        void enqueueDrawCommand(std::function<void(VkCommandBuffer)> drawAction);
        void enqueueDefaultPassDrawCommand(std::function<void(VkCommandBuffer)> drawAction);

        void flushResourceUpdates();

        void drawFrame();

        void finish();

        void fillDeviceBuffer(VulkanBuffer* buffer, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

        template <typename T>
        inline void fillDeviceBuffer(VulkanBuffer* buffer, const std::vector<T>& data, VkDeviceSize offset = 0)
        {
            fillDeviceBuffer(buffer, data.data(), data.size() * sizeof(T), offset);
        }

        void scheduleBufferForRemoval(std::shared_ptr<VulkanBuffer> buffer, uint32_t framesToLive = NumVirtualFrames);

        void setSceneImageView(const VulkanRenderPass* renderPass, uint32_t renderTargetIndex);

        void registerStreamingUniformBuffer(UniformBuffer* buffer);
        void unregisterStreamingUniformBuffer(UniformBuffer* buffer);

        Geometry* getFullScreenGeometry() const;

    private:
        void loadShaders(const std::filesystem::path& directoryPath);
        VkCommandBuffer         acquireCommandBuffer();
        std::optional<uint32_t> acquireSwapImageIndex();
        void resetCommandBuffer(VkCommandBuffer cmdBuffer);
        void record(VkCommandBuffer commandBuffer);
        void present(uint32_t VulkanSwapChainImageIndex);

        void recreateSwapChain();

        void destroyResourcesScheduledForRemoval();

        uint32_t m_numFramesRendered;
        uint32_t m_currentFrameIndex;
        std::filesystem::path m_resourcesPath;

        std::unique_ptr<VulkanContext>     m_context;
        std::unique_ptr<VulkanDevice>      m_device;
        std::unique_ptr<VulkanSwapChain>   m_swapChain;
        std::unique_ptr<DefaultRenderPass> m_defaultRenderPass;

        VkViewport m_defaultViewport;
        VkRect2D   m_defaultScissor;

        struct FrameResources
        {
            VkCommandPool   cmdPool;
            VkCommandBuffer cmdBuffer;
            VkFence         bufferFinishedFence;
            VkSemaphore     imageAvailableSemaphore;
            VkSemaphore     renderFinishedSemaphore;
        };
        std::array<FrameResources, NumVirtualFrames> m_frameResources;

        std::unordered_map<std::string, VkShaderModule> m_shaderModules;

        using FunctionVector = std::vector<std::function<void(VkCommandBuffer)>>;
        FunctionVector m_resourceUpdates;
        FunctionVector m_drawCommands;
        FunctionVector m_defaultPassDrawCommands;

        std::unique_ptr<Geometry> m_fullScreenGeometry;

        std::unordered_map<std::shared_ptr<VulkanBuffer>, uint32_t> m_removedBuffers;
        std::unordered_set<UniformBuffer*> m_streamingUniformBuffers;

        std::unique_ptr<VulkanPipeline>  m_scenePipeline;
        std::unique_ptr<VulkanSampler>   m_linearClampSampler;
        std::unique_ptr<Material>        m_sceneMaterial;

        std::vector<VulkanImageView*> m_sceneImageViews;
    };
}