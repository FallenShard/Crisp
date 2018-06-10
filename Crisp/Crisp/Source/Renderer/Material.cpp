#include "Material.hpp"

#include "vulkan/VulkanPipeline.hpp"
#include "vulkan/VulkanDevice.hpp"

namespace crisp
{
    Material::Material(VulkanPipeline* pipeline, std::vector<uint32_t> sharedSetIds, std::vector<uint32_t> uniqueSetIds)
        : m_pipeline(pipeline)
    {
        std::vector<VkDescriptorSet> sharedSets;
        for (auto setId : sharedSetIds)
        {
            if (setId >= sharedSets.size())
                sharedSets.resize(setId + 1);

            sharedSets[setId] = pipeline->allocateDescriptorSet(setId).getHandle();
        }

        for (auto& setVec : m_sets)
        {
            setVec = sharedSets;
            for (auto setId : uniqueSetIds)
            {
                if (setId >= setVec.size())
                    setVec.resize(setId + 1);

                setVec[setId] = pipeline->allocateDescriptorSet(setId).getHandle();
            }
        }
    }

    VkWriteDescriptorSet Material::makeDescriptorWrite(uint32_t setIndex, uint32_t binding, uint32_t frameIdx)
    {
        VkWriteDescriptorSet write = {};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = m_sets[frameIdx][setIndex];
        write.dstBinding      = binding;
        write.dstArrayElement = 0;
        write.descriptorType  = m_pipeline->getDescriptorType(setIndex, binding);
        write.descriptorCount = 1;
        return write;
    }

    void Material::setDynamicOffset(uint32_t frameIdx, uint32_t index, uint32_t offset)
    {
        if (m_dynamicOffsets[frameIdx].size() <= index)
            m_dynamicOffsets[frameIdx].resize(index + 1);

        m_dynamicOffsets[frameIdx][index] = offset;
    }

    void Material::bind(uint32_t frameIdx, VkCommandBuffer cmdBuffer)
    {
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getPipelineLayout()->getHandle(),
            0, static_cast<uint32_t>(m_sets[frameIdx].size()), m_sets[frameIdx].data(),
            static_cast<uint32_t>(m_dynamicOffsets[frameIdx].size()), m_dynamicOffsets[frameIdx].data());
    }
}