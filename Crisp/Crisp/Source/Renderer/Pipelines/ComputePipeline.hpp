#pragma once

#include "Vulkan/VulkanPipeline.hpp"

namespace crisp
{
    class ComputePipeline : public VulkanPipeline
    {
    public:
        ComputePipeline(VulkanRenderer* renderer, std::string&& shaderName, uint32_t numDynamicStorageBuffers, uint32_t numDescriptorSets, std::size_t pushConstantSize, const glm::uvec3& workGroupSize);

        const glm::uvec3& getWorkGroupSize() const;

    protected:
        virtual void create(int width, int height) override;

        VkShaderModule m_shader;
        glm::uvec3 m_workGroupSize;
    };
}
