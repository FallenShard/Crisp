#pragma once

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/vulkan/VulkanPipeline.hpp>
#include <Crisp/vulkan/VulkanSampler.hpp>
#include <Crisp/vulkan/VulkanImage.hpp>
#include <Crisp/vulkan/VulkanImageView.hpp>

#include <Crisp/Renderer/Material.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>
#include <Crisp/Renderer/RenderNode.hpp>
#include <Crisp/Renderer/DescriptorSetAllocator.hpp>

#include <CrispCore/RobinHood.hpp>

#include <memory>

namespace crisp
{
    class Renderer;
    class DescriptorSetAllocator;

    class ResourceContext
    {
    public:
        ResourceContext(Renderer* renderer);

        template <typename T>
        UniformBuffer* createUniformBuffer(std::string id, const std::vector<T>& data, BufferUpdatePolicy updatePolicy)
        {
            m_uniformBuffers[id] = std::make_unique<UniformBuffer>(m_renderer, data.size() * sizeof(T), updatePolicy, data.data());
            return m_uniformBuffers[id].get();
        }

        UniformBuffer*  createUniformBuffer(std::string id, VkDeviceSize size, BufferUpdatePolicy updatePolicy);
        VulkanPipeline* createPipeline(std::string id, std::string_view luaFilename, const VulkanRenderPass& renderPass, int subpassIndex);
        Material*       createMaterial(std::string materialId, std::string pipelineId);
        Material*       createMaterial(std::string materialId, VulkanPipeline* pipeline);

        void addUniformBuffer(std::string id, std::unique_ptr<UniformBuffer> uniformBuffer);
        void addGeometry(std::string id, std::unique_ptr<Geometry> geometry);
        void addImageWithView(std::string key, std::unique_ptr<VulkanImage> image, VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_2D);
        void addImageWithView(std::string key, std::unique_ptr<VulkanImage> image, std::unique_ptr<VulkanImageView> imageView);
        void addSampler(std::string id, std::unique_ptr<VulkanSampler> sampler);
        void addImage(std::string id, std::unique_ptr<VulkanImage> image);
        void addImageView(std::string id, std::unique_ptr<VulkanImageView> imageView);

        RenderNode* createPostProcessingEffectNode(std::string renderNodeId, std::string pipelineLuaFilename, const VulkanRenderPass& renderPass, const std::string& renderPassName);

        Geometry*        getGeometry(std::string id);
        Material*        getMaterial(std::string id);
        UniformBuffer*   getUniformBuffer(std::string id);
        VulkanSampler*   getSampler(std::string id);
        VulkanImage*     getImage(std::string id);
        VulkanImageView* getImageView(std::string id);

        void recreatePipelines();

        inline DescriptorSetAllocator* getDescriptorAllocator(VulkanPipelineLayout* pipelineLayout)
        {
            return m_descriptorAllocators.at(pipelineLayout).get();
        }

        inline const robin_hood::unordered_flat_map<std::string, std::unique_ptr<RenderNode>>& getRenderNodes() const
        {
            return m_renderNodes;
        }

    private:
        Renderer* m_renderer;

        robin_hood::unordered_flat_map<std::string, std::unique_ptr<Material>> m_materials;
        robin_hood::unordered_flat_map<std::string, std::unique_ptr<Geometry>> m_geometries;

        struct PipelineInfo
        {
            std::string luaFilename;
            const VulkanRenderPass* renderPass;
            int subpassIndex;
        };
        robin_hood::unordered_flat_map<std::string, PipelineInfo> m_pipelineInfos;
        robin_hood::unordered_flat_map<std::string, std::unique_ptr<VulkanPipeline>> m_pipelines;

        robin_hood::unordered_flat_map<std::string, std::unique_ptr<UniformBuffer>>  m_uniformBuffers;

        robin_hood::unordered_flat_map<std::string, std::unique_ptr<VulkanImage>>     m_images;
        robin_hood::unordered_flat_map<std::string, std::unique_ptr<VulkanImageView>> m_imageViews;
        robin_hood::unordered_flat_map<std::string, std::unique_ptr<VulkanSampler>>   m_samplers;

        robin_hood::unordered_flat_map<VulkanPipelineLayout*, std::unique_ptr<DescriptorSetAllocator>> m_descriptorAllocators;

        robin_hood::unordered_flat_map<std::string, std::unique_ptr<RenderNode>> m_renderNodes;
    };
}