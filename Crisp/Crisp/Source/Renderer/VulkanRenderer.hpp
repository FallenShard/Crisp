#pragma once

#include <memory>
#include <array>
#include <vector>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include <vulkan/vulkan.h>

#include "Vulkan/VulkanContext.hpp"
#include "Renderer/VertexBufferBindingGroup.hpp"
#include "Renderer/RenderPasses/DefaultRenderPass.hpp"

namespace crisp
{
    class VulkanImage;
    class VulkanDevice;
    class VulkanSwapChain;
    class VulkanRenderPass;
    class VulkanPipeline;
    class FullScreenQuadPipeline;

    class Texture;
    class TextureView;
    class VertexBuffer;
    class IndexBuffer;

    class VulkanRenderer
    {
    public:
        static constexpr uint32_t DefaultRenderPassId = 127;
        static constexpr unsigned int NumVirtualFrames = 3;

        VulkanRenderer(SurfaceCreator surfCreatorCallback, std::vector<std::string>&& extensions);
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer& other) = delete;
        VulkanRenderer(VulkanRenderer&& other) = delete;
        VulkanRenderer& operator=(const VulkanRenderer& other) = delete;
        VulkanRenderer& operator=(VulkanRenderer&& other) = delete;

        VulkanContext*    getContext() const;
        VulkanDevice*     getDevice() const;
        VulkanSwapChain*  getSwapChain() const;
        VkExtent2D        getSwapChainExtent() const;

        VulkanRenderPass* getDefaultRenderPass() const;
        VkViewport        getDefaultViewport() const;

        VkShaderModule    getShaderModule(std::string&& key) const;

        void setDefaultViewport(VkCommandBuffer cmdBuffer) const;
        void drawFullScreenQuad(VkCommandBuffer cmdBuffer) const;

        uint32_t getCurrentVirtualFrameIndex() const;

        void resize(int width, int height);

        void registerPipeline(VulkanPipeline* pipeline);
        void unregisterPipeline(VulkanPipeline* pipeline);

        void enqueueResourceUpdate(std::function<void(VkCommandBuffer)> imageTransition);
        void enqueueDrawCommand(std::function<void(VkCommandBuffer)> drawAction);
        void enqueueDefaultPassDrawCommand(std::function<void(VkCommandBuffer)> drawAction);

        void flushResourceUpdates();

        void drawFrame();
        
        void finish();

        void fillDeviceBuffer(VulkanBuffer* buffer, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

        void scheduleBufferForRemoval(std::shared_ptr<VulkanBuffer> buffer, uint32_t framesToLive = NumVirtualFrames);

    private:
        void loadShaders(std::string dirPath);

        VkCommandBuffer         acquireCommandBuffer();
        std::optional<uint32_t> acquireSwapChainImageIndex();
        void            resetCommandBuffer(VkCommandBuffer cmdBuffer);
        VkFramebuffer   recreateFramebuffer(uint32_t swapChainImgIndex);
        void            record(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);
        void            present(uint32_t swapChainImageIndex);

        void recreateSwapChain();

        void destroyResourcesScheduledForRemoval();

        std::unique_ptr<VulkanContext> m_context;
        std::unique_ptr<VulkanDevice>  m_device;
        std::unique_ptr<VulkanSwapChain> m_swapChain;
        std::unique_ptr<VulkanRenderPass> m_defaultRenderPass;

        std::unique_ptr<Texture> m_depthTexture;
        std::vector<std::shared_ptr<TextureView>> m_depthTexViews;

        VkViewport m_defaultViewport;

        uint32_t m_framesRendered;
        uint32_t m_currentFrameIndex;

        struct FrameResources
        {
            VkCommandPool   cmdPool;
            VkCommandBuffer cmdBuffer;
            VkFence         bufferFinishedFence;
            VkSemaphore     imageAvailableSemaphore;
            VkSemaphore     renderFinishedSemaphore;
            VkFramebuffer   framebuffer;
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
        
        std::unordered_set<VulkanPipeline*> m_pipelines;

        std::unordered_map<std::shared_ptr<VulkanBuffer>, uint32_t> m_removedBuffers;
    };
}