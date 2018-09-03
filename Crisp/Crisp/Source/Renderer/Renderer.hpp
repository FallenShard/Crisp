#pragma once

#include <memory>
#include <array>
#include <vector>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>

#include <vulkan/vulkan.h>

#include "Vulkan/VulkanContext.hpp"
#include "Renderer/VertexBufferBindingGroup.hpp"
#include "Renderer/RenderPasses/DefaultRenderPass.hpp"
#include "DescriptorSetGroup.hpp"

namespace crisp
{
    class VulkanSampler;
    class VulkanImage;
    class VulkanDevice;
    class VulkanSwapChain;
    class VulkanPipeline;

    class Geometry;
    class Texture;
    class VulkanImageView;
    class VertexBuffer;
    class IndexBuffer;
    class UniformBuffer;

    class Renderer
    {
    public:
        static constexpr unsigned int NumVirtualFrames = 3;

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

        Geometry*          getFullScreenGeometry() const;

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

        void setSceneImageView(std::unique_ptr<VulkanImageView> sceneImageView);

        void registerStreamingUniformBuffer(UniformBuffer* buffer);
        void unregisterStreamingUniformBuffer(UniformBuffer* buffer);

    private:
        void loadShaders(const std::filesystem::path& directoryPath);
        VkCommandBuffer         acquireCommandBuffer();
        std::optional<uint32_t> acquireSwapChainImageIndex();
        void resetCommandBuffer(VkCommandBuffer cmdBuffer);
        void record(VkCommandBuffer commandBuffer);
        void present(uint32_t swapChainImageIndex);

        void recreateSwapChain();

        void destroyResourcesScheduledForRemoval();

        std::unique_ptr<VulkanContext>     m_context;
        std::unique_ptr<VulkanDevice>      m_device;
        std::unique_ptr<VulkanSwapChain>   m_swapChain;
        std::unique_ptr<DefaultRenderPass> m_defaultRenderPass;

        VkViewport m_defaultViewport;
        VkRect2D m_defaultScissor;

        uint32_t m_framesRendered;
        uint32_t m_currentFrameIndex;
        std::filesystem::path m_resourcesPath;

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

        std::unique_ptr<VertexBuffer> m_fsQuadVertexBuffer;
        std::unique_ptr<IndexBuffer>  m_fsQuadIndexBuffer;
        VertexBufferBindingGroup      m_fsQuadVertexBufferBindingGroup;
        std::unique_ptr<Geometry> m_fullScreenGeometry;

        std::unordered_map<std::shared_ptr<VulkanBuffer>, uint32_t> m_removedBuffers;
        std::unordered_set<UniformBuffer*> m_streamingUniformBuffers;

        std::unique_ptr<VulkanPipeline>  m_scenePipeline;
        std::unique_ptr<VulkanSampler>   m_linearClampSampler;
        std::unique_ptr<VulkanImageView> m_sceneImageView;
        DescriptorSetGroup               m_sceneDescSetGroup;
    };
}