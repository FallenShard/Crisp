#pragma once

#include "VirtualFrame.hpp"

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
    class VulkanCommandPool;
    class VulkanCommandBuffer;

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
        std::filesystem::path getShaderSourcePath(const std::string& shaderName) const;

        VulkanContext*    getContext() const;
        VulkanDevice*     getDevice() const;
        VulkanSwapChain*  getSwapChain() const;
        VkExtent2D        getSwapChainExtent() const;

        DefaultRenderPass* getDefaultRenderPass() const;
        VkViewport         getDefaultViewport() const;
        VkRect2D           getDefaultScissor() const;

        VkShaderModule loadShaderModule(const std::string& key);
        VkShaderModule compileGlsl(const std::filesystem::path& path, std::string id, std::string type);
        VkShaderModule getShaderModule(const std::string& key) const;

        void setDefaultViewport(VkCommandBuffer cmdBuffer) const;
        void setDefaultScissor(VkCommandBuffer cmdBuffer) const;
        void drawFullScreenQuad(VkCommandBuffer cmdBuffer) const;

        uint32_t getCurrentVirtualFrameIndex() const;

        void resize(int width, int height);

        void enqueueResourceUpdate(std::function<void(VkCommandBuffer)> resourceUpdate);
        void enqueueDrawCommand(std::function<void(VkCommandBuffer)> drawAction);
        void enqueueDefaultPassDrawCommand(std::function<void(VkCommandBuffer)> drawAction);

        void flushResourceUpdates(bool waitOnAllQueues);

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
        void setSceneImageViews(const std::vector<std::unique_ptr<VulkanImageView>>& imageViews);

        void registerStreamingUniformBuffer(UniformBuffer* buffer);
        void unregisterStreamingUniformBuffer(UniformBuffer* buffer);

        Geometry* getFullScreenGeometry() const;

        std::unique_ptr<VulkanPipeline> createPipelineFromLua(std::string_view pipelineName, const VulkanRenderPass& renderPass, int subpassIndex);

        template <typename ...Args>
        std::unique_ptr<UniformBuffer> createUniformBuffer(Args&&... args)
        {
            return std::make_unique<UniformBuffer>(this, std::forward<Args>(args)...);
        }

    private:
        void loadShaders(const std::filesystem::path& directoryPath);
        VkShaderModule loadSpirvShaderModule(const std::filesystem::path& shaderModulePath);
        std::optional<uint32_t> acquireSwapImageIndex(VirtualFrame& virtualFrame);
        void resetCommandBuffer(VkCommandBuffer cmdBuffer);
        void record(VkCommandBuffer commandBuffer);
        void present(VirtualFrame& virtualFrame, uint32_t swapChainImageIndex);

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

        std::unordered_map<std::shared_ptr<VulkanBuffer>, uint32_t> m_removedBuffers;
        std::unordered_set<UniformBuffer*> m_streamingUniformBuffers;

        std::unique_ptr<VulkanPipeline>  m_scenePipeline;
        std::unique_ptr<VulkanSampler>   m_linearClampSampler;
        std::unique_ptr<Material>        m_sceneMaterial;

        std::vector<VulkanImageView*> m_sceneImageViews;
    };
}