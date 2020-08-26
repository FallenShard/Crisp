#include "RayTracingMaterial.hpp"

#include "Renderer/Renderer.hpp"

#include <CrispCore/Math/Headers.hpp>
#include "Renderer/UniformBuffer.hpp"

namespace crisp
{
    namespace
    {
        struct PushConst
        {
            glm::vec4 clearColor;
            glm::vec3 lightPos;
            float lightIntensity;
            int lightType;
        };

        uint32_t cameraParamSize = 0;
    }

    RayTracingMaterial::RayTracingMaterial(Renderer* renderer, VkAccelerationStructureNV tlas, std::array<VkImageView, 2> writeImageViews, const UniformBuffer& cameraBuffer)
        : m_renderer(renderer)
    {
        auto device = renderer->getDevice()->getHandle();

        VkDescriptorSetLayoutBinding layoutBinding = {};
        layoutBinding.binding = 0;
        layoutBinding.descriptorCount = 1;
        layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
        layoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

        VkDescriptorSetLayoutBinding binding2 = {};
        binding2.binding = 1;
        binding2.descriptorCount = 1;
        binding2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        binding2.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

        VkDescriptorSetLayoutBinding binding3 = {};
        binding3.binding = 2;
        binding3.descriptorCount = 1;
        binding3.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        binding3.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;
        VkDescriptorSetLayoutBinding bindings[] = { layoutBinding, binding2, binding3 };

        VkDescriptorPoolSize poolSizes[] =
        {
            { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 2},
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2}
        };

        VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.maxSets = 2;
        poolInfo.poolSizeCount = 3;
        poolInfo.pPoolSizes = poolSizes;
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_pool);

        VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        layoutInfo.bindingCount = 3;
        layoutInfo.pBindings = bindings;
        m_setLayouts.resize(1);
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_setLayouts[0]);

        m_descSets.resize(2);
        VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool = m_pool;
        VkDescriptorSetLayout allocatedLayouts[] = { m_setLayouts[0], m_setLayouts[0] };
        allocInfo.descriptorSetCount = 2;
        allocInfo.pSetLayouts = allocatedLayouts;
        vkAllocateDescriptorSets(device, &allocInfo, m_descSets.data());

        VkWriteDescriptorSetAccelerationStructureNV writeSetAS = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV };
        writeSetAS.accelerationStructureCount = 1;
        writeSetAS.pAccelerationStructures = &tlas;

        for (int i = 0; i < 2; ++i)
        {
            VkWriteDescriptorSet writeSet0 = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            writeSet0.dstSet          = m_descSets[i];
            writeSet0.pNext           = &writeSetAS;
            writeSet0.dstBinding      = 0;
            writeSet0.descriptorCount = 1;
            writeSet0.descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;

            VkDescriptorImageInfo imageInfo;
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfo.imageView   = writeImageViews[i];
            imageInfo.sampler     = nullptr;
            VkWriteDescriptorSet writeSet1 = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            writeSet1.dstSet          = m_descSets[i];
            writeSet1.dstBinding      = 1;
            writeSet1.descriptorCount = 1;
            writeSet1.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writeSet1.pImageInfo      = &imageInfo;

            VkDescriptorBufferInfo bufferInfo = cameraBuffer.getDescriptorInfo();
            VkWriteDescriptorSet writeSet3 = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            writeSet3.dstSet          = m_descSets[i];
            writeSet3.dstBinding      = 2;
            writeSet3.descriptorCount = 1;
            writeSet3.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            writeSet3.pBufferInfo     = &bufferInfo;

            cameraParamSize = bufferInfo.range;

            VkWriteDescriptorSet writeSets[] = { writeSet0, writeSet1, writeSet3 };
            vkUpdateDescriptorSets(device, 3, writeSets, 0, nullptr);
        }

        createPipeline(renderer);
    }

    void RayTracingMaterial::bind(VkCommandBuffer cmdBuffer, uint32_t virtualFrameIndex) const
    {
        PushConst pushConstants = {};
        pushConstants.clearColor = glm::vec4(1, 0, 0, 1);

        uint32_t offset = virtualFrameIndex * cameraParamSize;

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_pipeline);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_pipelineLayout,
            0, 1, &m_descSets[virtualFrameIndex], 1, &offset);
        vkCmdPushConstants(cmdBuffer, m_pipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_NV |
            VK_SHADER_STAGE_MISS_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, 0, sizeof(PushConst), &pushConstants);
    }

    VkExtent2D RayTracingMaterial::getExtent() const
    {
        return m_renderer->getSwapChainExtent();
    }

    VulkanBuffer* RayTracingMaterial::getShaderBindingTableBuffer() const
    {
        return m_sbtBuffer.get();
    }

    uint32_t RayTracingMaterial::getShaderGroupHandleSize() const
    {
        return m_renderer->getContext()->getPhysicalDeviceRayTracingProperties().shaderGroupHandleSize;
    }

    void RayTracingMaterial::createPipeline(Renderer* renderer)
    {
        VkShaderModule raygenSM = renderer->compileGlsl("raytrace.rgen",  "raytrace-rgen",  "rgen");
        VkShaderModule missSM   = renderer->compileGlsl("raytrace.rmiss", "raytrace-rmiss", "rmiss");
        VkShaderModule rchitSM  = renderer->compileGlsl("raytrace.rchit", "raytrace-rchit", "rchit");

        std::vector<VkPipelineShaderStageCreateInfo> stages;
        std::vector< VkRayTracingShaderGroupCreateInfoNV> groups;

        VkRayTracingShaderGroupCreateInfoNV rtGroup1 = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV };
        rtGroup1.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
        rtGroup1.anyHitShader       = VK_SHADER_UNUSED_NV;
        rtGroup1.closestHitShader   = VK_SHADER_UNUSED_NV;
        rtGroup1.generalShader      = VK_SHADER_UNUSED_NV;
        rtGroup1.intersectionShader = VK_SHADER_UNUSED_NV;
        VkPipelineShaderStageCreateInfo info1 = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        info1.stage = VK_SHADER_STAGE_RAYGEN_BIT_NV;
        info1.module = raygenSM;
        info1.pName = "main";
        stages.push_back(info1);
        rtGroup1.generalShader = 0;
        groups.push_back(rtGroup1);

        VkRayTracingShaderGroupCreateInfoNV rtGroup2 = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV };
        rtGroup2.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
        rtGroup2.anyHitShader = VK_SHADER_UNUSED_NV;
        rtGroup2.closestHitShader = VK_SHADER_UNUSED_NV;
        rtGroup2.generalShader = VK_SHADER_UNUSED_NV;
        rtGroup2.intersectionShader = VK_SHADER_UNUSED_NV;
        VkPipelineShaderStageCreateInfo info2 = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        info2.stage = VK_SHADER_STAGE_MISS_BIT_NV;
        info2.module = missSM;
        info2.pName = "main";
        stages.push_back(info2);
        rtGroup2.generalShader = 1;
        groups.push_back(rtGroup2);

        VkRayTracingShaderGroupCreateInfoNV rtGroup3 = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV };
        rtGroup3.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
        rtGroup3.anyHitShader = VK_SHADER_UNUSED_NV;
        rtGroup3.closestHitShader = VK_SHADER_UNUSED_NV;
        rtGroup3.generalShader = VK_SHADER_UNUSED_NV;
        rtGroup3.intersectionShader = VK_SHADER_UNUSED_NV;
        VkPipelineShaderStageCreateInfo info3 = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        info3.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;
        info3.module = rchitSM;
        info3.pName = "main";
        stages.push_back(info3);
        rtGroup3.closestHitShader = 2;
        groups.push_back(rtGroup3);

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

        VkPushConstantRange pcRange;
        pcRange.offset = 0;
        pcRange.size = sizeof(PushConst);
        pcRange.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_MISS_BIT_NV | VK_SHADER_STAGE_RAYGEN_BIT_NV;
        pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
        pipelineLayoutCreateInfo.pPushConstantRanges = &pcRange;
        pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)m_setLayouts.size();
        pipelineLayoutCreateInfo.pSetLayouts = m_setLayouts.data();
        vkCreatePipelineLayout(renderer->getDevice()->getHandle(), &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);


        auto createPipelineFunc = (PFN_vkCreateRayTracingPipelinesNV)(vkGetDeviceProcAddr(renderer->getDevice()->getHandle(),
            "vkCreateRayTracingPipelinesNV"));
        VkRayTracingPipelineCreateInfoNV raytracingCreateInfo = { VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV };
        raytracingCreateInfo.stageCount        = static_cast<uint32_t>(stages.size());
        raytracingCreateInfo.pStages           = stages.data();
        raytracingCreateInfo.groupCount        = static_cast<uint32_t>(groups.size());
        raytracingCreateInfo.pGroups           = groups.data();
        raytracingCreateInfo.maxRecursionDepth = 1;
        raytracingCreateInfo.layout            = m_pipelineLayout;
        createPipelineFunc(renderer->getDevice()->getHandle(), nullptr, 1, &raytracingCreateInfo, nullptr, &m_pipeline);

        uint32_t groupCount = 3;
        uint32_t handleSize = renderer->getContext()->getPhysicalDeviceRayTracingProperties().shaderGroupHandleSize;

        uint32_t sbtSize = groupCount * handleSize;

        std::vector<uint8_t> shaderHandleStorage(sbtSize);
        auto getHandleFunc = (PFN_vkGetRayTracingShaderGroupHandlesNV)(vkGetDeviceProcAddr(renderer->getDevice()->getHandle(),
            "vkGetRayTracingShaderGroupHandlesNV"));
        getHandleFunc(renderer->getDevice()->getHandle(), m_pipeline, 0, groupCount,
            sbtSize, shaderHandleStorage.data());

        m_sbtBuffer = std::make_unique<VulkanBuffer>(renderer->getDevice(), (VkDeviceSize)sbtSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        renderer->enqueueResourceUpdate([&, this](VkCommandBuffer cmdBuffer)
        {
            vkCmdUpdateBuffer(cmdBuffer, m_sbtBuffer->getHandle(), 0, shaderHandleStorage.size(), shaderHandleStorage.data());
            vkDeviceWaitIdle(renderer->getDevice()->getHandle());
        });
        renderer->flushResourceUpdates(true);

        int x = 0;
        ++x;
    }
}
