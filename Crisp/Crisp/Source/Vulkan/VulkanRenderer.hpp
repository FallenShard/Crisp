#pragma once

#include <memory>
#include <array>
#include <vector>
#include <unordered_map>

#include <vulkan/vulkan.h>

#include "VulkanContext.hpp"
#include "VulkanDevice.hpp"
#include "VulkanSwapChain.hpp"
#include "RenderPasses/DefaultRenderPass.hpp"

#include "DrawItem.hpp"

namespace crisp
{
    struct DrawItem;
    class FullScreenQuadPipeline;

    class VulkanRenderer
    {
    public:
        static constexpr uint32_t DefaultRenderPassId = 127;
        static constexpr unsigned int NumVirtualFrames = 3;

        VulkanRenderer(SurfaceCreator surfCreatorCallback, std::vector<const char*>&& extensions);
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer& other) = delete;
        VulkanRenderer(VulkanRenderer&& other) = delete;
        VulkanRenderer& operator=(const VulkanRenderer& other) = delete;
        VulkanRenderer& operator=(VulkanRenderer&& other) = delete;

        VulkanContext& getContext();
        VulkanDevice& getDevice();
        VulkanSwapChain& getSwapChain();
        VulkanRenderPass& getDefaultRenderPass() const;
        VkShaderModule getShaderModule(std::string&& key) const;
        VkExtent2D getSwapChainExtent() const;

        VkBuffer getFullScreenQuadVertexBuffer() const;
        VkBuffer getFullScreenQuadIndexBuffer() const;
        uint32_t getCurrentFrameIndex() const;

        void resize(int width, int height);

        void registerRenderPass(uint32_t key, VulkanRenderPass* renderPass);
        void unregisterRenderPass(uint32_t key);

        void addImageTransition(std::function<void(VkCommandBuffer&)> imageTransition);
        void addCopyAction(std::function<void(VkCommandBuffer&)> copyAction);
        void addDrawAction(std::function<void(VkCommandBuffer&)> drawAction, uint32_t renderPassId);

        //void displayImage(VkImageView imageView, int numLayers);
        
        void drawFrame();
        
        void finish();

        void scheduleStagingBufferForRemoval(VkBuffer buffer);
        void destroyObjectsScheduledForRemoval();

    private:
        void loadShaders(std::string dirPath);

        VkCommandBuffer acquireCommandBuffer();
        uint32_t acquireNextImageIndex();
        void resetCommandBuffer(VkCommandBuffer cmdBuffer);
        VkFramebuffer recreateFramebuffer(uint32_t swapChainImgIndex);
        void record(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer);
        void submitToQueue(VkCommandBuffer cmdBuffer);
        void present(uint32_t swapChainImageIndex);

        void recreateSwapChain();

        std::unique_ptr<VulkanContext> m_context;
        std::unique_ptr<VulkanDevice>  m_device;
        std::unique_ptr<VulkanSwapChain> m_swapChain;
        std::unique_ptr<VulkanRenderPass> m_defaultRenderPass;

        VkFormat m_depthFormat;
        VkImage m_depthImageArray;
        std::vector<VkImageView> m_depthImageViews;

        uint32_t m_framesRendered;
        uint32_t m_currentFrameIndex;

        struct FrameResources
        {
            VkCommandBuffer cmdBuffer;
            VkFence bufferFinishedFence;
            VkSemaphore imageAvailableSemaphore;
            VkSemaphore renderFinishedSemaphore;
            VkFramebuffer framebuffer;
        };
        std::array<FrameResources, NumVirtualFrames> m_frameResources;

        std::unordered_map<std::string, VkShaderModule> m_shaderModules;

        
        std::vector<std::function<void(VkCommandBuffer&)>> m_renderActions;

        VkBuffer m_fsQuadVertexBuffer;
        VkBuffer m_fsQuadIndexBuffer;
        //VkSampler m_sampler;

        //VkImageView m_displayedImageView;
        //uint32_t m_displayedImageLayerCount;

        //std::unique_ptr<FullScreenQuadPipeline> m_fsQuadPipeline;
        //VkDescriptorSet m_fsQuadDescSet;

        using ActionVector = std::vector<std::function<void(VkCommandBuffer&)>>;
        ActionVector m_imageTransitions;
        ActionVector m_copyActions;
        std::map<uint32_t, std::pair<VulkanRenderPass*, ActionVector>> m_renderPasses;

        std::map<VkBuffer, uint32_t> m_oldStagingBuffers;
    };
}