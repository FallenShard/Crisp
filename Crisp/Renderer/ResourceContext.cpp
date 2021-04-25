#include "ResourceContext.hpp"

#include "ShadingLanguage/ShaderCompiler.hpp"

#include "Core/ApplicationEnvironment.hpp"

namespace crisp
{
    ResourceContext::ResourceContext(Renderer* renderer)
        : m_renderer(renderer)
    {
    }

    UniformBuffer* ResourceContext::createUniformBuffer(std::string id, VkDeviceSize size, BufferUpdatePolicy updatePolicy)
    {
        m_uniformBuffers[id] = std::make_unique<UniformBuffer>(m_renderer, size, updatePolicy);
        return m_uniformBuffers[id].get();
    }

    VulkanPipeline* ResourceContext::createPipeline(std::string id, std::string_view luaFilename, const VulkanRenderPass& renderPass, int subpassIndex)
    {
        m_pipelineInfos[id].luaFilename  = std::string(luaFilename);
        m_pipelineInfos[id].renderPass   = &renderPass;
        m_pipelineInfos[id].subpassIndex = subpassIndex;
        auto pipeline = m_renderer->createPipelineFromLua(luaFilename, renderPass, subpassIndex);
        m_pipelines[id] = std::move(pipeline);
        m_pipelines[id]->setTag(id);
        return m_pipelines.at(id).get();
        return nullptr;
    }

    Material* ResourceContext::createMaterial(std::string materialId, std::string pipelineId)
    {
        m_materials[materialId] = std::make_unique<Material>(m_pipelines[pipelineId].get());
        return m_materials.at(materialId).get();
    }

    Material* ResourceContext::createMaterial(std::string materialId, VulkanPipeline* pipeline)
    {
        m_materials[materialId] = std::make_unique<Material>(pipeline);
        return m_materials.at(materialId).get();
    }

    void ResourceContext::addUniformBuffer(std::string id, std::unique_ptr<UniformBuffer> uniformBuffer)
    {
        m_uniformBuffers[id] = std::move(uniformBuffer);
    }

    void ResourceContext::addGeometry(std::string id, std::unique_ptr<Geometry> geometry)
    {
        m_geometries[id] = std::move(geometry);
    }

    void ResourceContext::addImageWithView(std::string key, std::unique_ptr<VulkanImage> image, VkImageViewType imageViewType)
    {
        m_imageViews[key] = image->createView(imageViewType);
        m_images[key] = std::move(image);
    }

    void ResourceContext::addImageWithView(std::string key, std::unique_ptr<VulkanImage> image, std::unique_ptr<VulkanImageView> imageView)
    {
        m_imageViews[key] = std::move(imageView);
        m_images[key] = std::move(image);
    }

    void ResourceContext::addSampler(std::string id, std::unique_ptr<VulkanSampler> sampler)
    {
        m_samplers[id] = std::move(sampler);
    }

    void ResourceContext::addImage(std::string id, std::unique_ptr<VulkanImage> image)
    {
        m_images[id] = std::move(image);
    }

    void ResourceContext::addImageView(std::string id, std::unique_ptr<VulkanImageView> imageView)
    {
        m_imageViews[id] = std::move(imageView);
    }

    Geometry* ResourceContext::getGeometry(std::string id)
    {
        return m_geometries.at(id).get();
    }

    Material* ResourceContext::getMaterial(std::string id)
    {
        return m_materials.at(id).get();
    }

    UniformBuffer* ResourceContext::getUniformBuffer(std::string id)
    {
        return m_uniformBuffers[id].get();
    }

    VulkanSampler* ResourceContext::getSampler(std::string id)
    {
        return m_samplers[id].get();
    }

    VulkanImage* ResourceContext::getImage(std::string id)
    {
        return m_images[id].get();
    }

    VulkanImageView* ResourceContext::getImageView(std::string id)
    {
        return m_imageViews[id].get();
    }

    void ResourceContext::recreatePipelines()
    {
        ShaderCompiler compiler;
        compiler.compileDir(ApplicationEnvironment::getShaderSourcesPath(),
            ApplicationEnvironment::getResourcesPath() / "Shaders");

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer buffer)
        {
            for (auto& [id, info] : m_pipelineInfos)
            {
                auto pipeline = m_renderer->createPipelineFromLua(info.luaFilename, *info.renderPass, info.subpassIndex);
                m_pipelines[id]->swap(*pipeline);
            }
        });
    }
}
