#pragma once

#include "Material.hpp"
#include "UniformBuffer.hpp"
#include "Geometry/Geometry.hpp"
#include "vulkan/VulkanPipeline.hpp"
#include "vulkan/VulkanSampler.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanImageView.hpp"

#include "RenderNode.hpp"

#include <memory>
#include <unordered_map>

namespace crisp
{
    class Renderer;

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

        Geometry*        getGeometry(std::string id);
        Material*        getMaterial(std::string id);
        UniformBuffer*   getUniformBuffer(std::string id);
        VulkanSampler*   getSampler(std::string id);
        VulkanImage*     getImage(std::string id);
        VulkanImageView* getImageView(std::string id);

        void recreatePipelines();

    private:
        Renderer* m_renderer;

        std::unordered_map<std::string, std::unique_ptr<Material>> m_materials;
        std::unordered_map<std::string, std::unique_ptr<Geometry>> m_geometries;

        struct PipelineInfo
        {
            std::string luaFilename;
            const VulkanRenderPass* renderPass;
            int subpassIndex;
        };
        std::unordered_map<std::string, PipelineInfo> m_pipelineInfos;
        std::unordered_map<std::string, std::unique_ptr<VulkanPipeline>> m_pipelines;

        std::unordered_map<std::string, std::unique_ptr<UniformBuffer>>  m_uniformBuffers;

        std::unordered_map<std::string, std::unique_ptr<VulkanImage>>     m_images;
        std::unordered_map<std::string, std::unique_ptr<VulkanImageView>> m_imageViews;
        std::unordered_map<std::string, std::unique_ptr<VulkanSampler>>   m_samplers;
    };
}