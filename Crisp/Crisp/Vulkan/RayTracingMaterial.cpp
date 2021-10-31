#include "RayTracingMaterial.hpp"

#include <Crisp/Renderer/Renderer.hpp>

#include <CrispCore/Math/Headers.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>

#include <Crisp/Geometry/Geometry.hpp>

#define GET_DEVICE_PROC_NV(procVar, device) getDeviceProc(procVar, device, #procVar "NV")

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

        template <typename T>
        void getDeviceProc(T& procRef, VulkanDevice* device, const char* procName)
        {
            procRef = reinterpret_cast<T>(vkGetDeviceProcAddr(device->getHandle(), procName));
        }

        PFN_vkCreateRayTracingPipelinesNV       vkCreateRayTracingPipelines;
        PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandles;
    }

    RayTracingMaterial::RayTracingMaterial(Renderer* renderer, VkAccelerationStructureNV tlas, std::array<VkImageView, 2> writeImageViews, const UniformBuffer& cameraBuffer)
        : m_renderer(renderer)
    {
        auto device = renderer->getDevice()->getHandle();

        VkDescriptorSetLayoutBinding accelStructBinding = {};
        accelStructBinding.binding         = 0;
        accelStructBinding.descriptorCount = 1;
        accelStructBinding.descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
        accelStructBinding.stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_NV;

        VkDescriptorSetLayoutBinding renderedImageBinding = {};
        renderedImageBinding.binding         = 1;
        renderedImageBinding.descriptorCount = 1;
        renderedImageBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        renderedImageBinding.stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_NV;

        VkDescriptorSetLayoutBinding cameraBinding = {};
        cameraBinding.binding         = 2;
        cameraBinding.descriptorCount = 1;
        cameraBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        cameraBinding.stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_NV;
        VkDescriptorSetLayoutBinding bindings[] = { accelStructBinding, renderedImageBinding, cameraBinding };

        VkDescriptorSetLayoutBinding vertexBufferBinding = {};
        vertexBufferBinding.binding         = 0;
        vertexBufferBinding.descriptorCount = 6;
        vertexBufferBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        vertexBufferBinding.stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;

        VkDescriptorSetLayoutBinding indexBufferBinding = {};
        indexBufferBinding.binding         = 1;
        indexBufferBinding.descriptorCount = 6;
        indexBufferBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        indexBufferBinding.stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;

        VkDescriptorSetLayoutBinding randBufferBinding = {};
        randBufferBinding.binding         = 2;
        randBufferBinding.descriptorCount = 1;
        randBufferBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        randBufferBinding.stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;

        VkDescriptorSetLayoutBinding secondSetBindings[] = { vertexBufferBinding, indexBufferBinding, randBufferBinding };

        VkDescriptorPoolSize poolSizes[] =
        {
            { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, Renderer::NumVirtualFrames },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Renderer::NumVirtualFrames },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, Renderer::NumVirtualFrames },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 }
        };

        VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.maxSets       = Renderer::NumVirtualFrames + 1;
        poolInfo.poolSizeCount = 4;
        poolInfo.pPoolSizes    = poolSizes;
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_pool);

        VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        layoutInfo.bindingCount = 3;
        layoutInfo.pBindings    = bindings;
        m_setLayouts.resize(2);
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_setLayouts[0]);

        layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        layoutInfo.bindingCount = 3;
        layoutInfo.pBindings    = secondSetBindings;
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_setLayouts[1]);

        m_descSets.resize(Renderer::NumVirtualFrames * 2);
        VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool     = m_pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &m_setLayouts[0];
        vkAllocateDescriptorSets(device, &allocInfo, &m_descSets[0]);

        allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool     = m_pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &m_setLayouts[0];
        vkAllocateDescriptorSets(device, &allocInfo, &m_descSets[2]);

        allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool     = m_pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &m_setLayouts[1];
        vkAllocateDescriptorSets(device, &allocInfo, &m_descSets[1]);
        m_descSets[3] = m_descSets[1];

        for (int i = 0; i < Renderer::NumVirtualFrames; ++i)
        {
            VkWriteDescriptorSetAccelerationStructureNV writeSetAS = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV };
            writeSetAS.accelerationStructureCount = 1;
            writeSetAS.pAccelerationStructures = &tlas;
            VkWriteDescriptorSet writeAsBinding = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            writeAsBinding.dstSet          = m_descSets[i * 2];
            writeAsBinding.pNext           = &writeSetAS;
            writeAsBinding.dstBinding      = 0;
            writeAsBinding.descriptorCount = 1;
            writeAsBinding.descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;

            VkDescriptorImageInfo imageInfo;
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfo.imageView   = writeImageViews[i];
            imageInfo.sampler     = nullptr;
            VkWriteDescriptorSet writeImageBinding = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            writeImageBinding.dstSet          = m_descSets[i * 2];
            writeImageBinding.dstBinding      = 1;
            writeImageBinding.descriptorCount = 1;
            writeImageBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writeImageBinding.pImageInfo      = &imageInfo;

            VkDescriptorBufferInfo bufferInfo = cameraBuffer.getDescriptorInfo();
            VkWriteDescriptorSet writeCameraBinding = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            writeCameraBinding.dstSet          = m_descSets[i * 2];
            writeCameraBinding.dstBinding      = 2;
            writeCameraBinding.descriptorCount = 1;
            writeCameraBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            writeCameraBinding.pBufferInfo     = &bufferInfo;

            cameraParamSize = static_cast<uint32_t>(bufferInfo.range);

            VkWriteDescriptorSet writeSets[] = { writeAsBinding, writeImageBinding, writeCameraBinding };
            vkUpdateDescriptorSets(device, 3, writeSets, 0, nullptr);
        }

        m_frameIdx = 0;
        createPipeline(renderer);
    }

    void RayTracingMaterial::bind(VkCommandBuffer cmdBuffer, uint32_t virtualFrameIndex) const
    {
        PushConst pushConstants = {};
        pushConstants.clearColor = glm::vec4(1, 0, 0, 1);

        uint32_t offset = virtualFrameIndex * cameraParamSize;

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_pipeline);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, m_pipelineLayout,
            0, 2, &m_descSets[virtualFrameIndex * 2], 1, &offset);
        vkCmdPushConstants(cmdBuffer, m_pipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV,
            0, sizeof(uint32_t), &m_frameIdx);

        ++m_frameIdx;
    }

    void RayTracingMaterial::updateGeometryBufferDescriptors(const Geometry& geometry, uint32_t idx)
    {
        auto* vtxBuffer = geometry.getVertexBuffer();
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = vtxBuffer->getHandle();
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;
        VkWriteDescriptorSet writeVertexBuffer = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        writeVertexBuffer.dstSet          = m_descSets[1];
        writeVertexBuffer.dstBinding      = 0;
        writeVertexBuffer.descriptorCount = 1;
        writeVertexBuffer.dstArrayElement = idx;
        writeVertexBuffer.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeVertexBuffer.pBufferInfo     = &bufferInfo;

        auto* idxBuffer = geometry.getIndexBuffer();
        VkDescriptorBufferInfo idxBufferInfo = {};
        idxBufferInfo.buffer = idxBuffer->getHandle();
        idxBufferInfo.offset = 0;
        idxBufferInfo.range = VK_WHOLE_SIZE;
        VkWriteDescriptorSet writeIndexBuffer = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        writeIndexBuffer.dstSet          = m_descSets[1];
        writeIndexBuffer.dstBinding      = 1;
        writeIndexBuffer.descriptorCount = 1;
        writeIndexBuffer.dstArrayElement = idx;
        writeIndexBuffer.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeIndexBuffer.pBufferInfo     = &idxBufferInfo;

        VkWriteDescriptorSet writeSets[] = { writeVertexBuffer, writeIndexBuffer };
        vkUpdateDescriptorSets(m_renderer->getDevice()->getHandle(), 2, writeSets, 0, nullptr);
    }

    void RayTracingMaterial::setRandomBuffer(const UniformBuffer& randBuffer)
    {
        VkDescriptorBufferInfo idxBufferInfo = {};
        idxBufferInfo.buffer = randBuffer.get();
        idxBufferInfo.offset = 0;
        idxBufferInfo.range = VK_WHOLE_SIZE;
        VkWriteDescriptorSet writeIndexBuffer = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        writeIndexBuffer.dstSet = m_descSets[1];
        writeIndexBuffer.dstBinding = 2;
        writeIndexBuffer.descriptorCount = 1;
        writeIndexBuffer.dstArrayElement = 0;
        writeIndexBuffer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeIndexBuffer.pBufferInfo = &idxBufferInfo;

        VkWriteDescriptorSet writeSets[] = { writeIndexBuffer };
        vkUpdateDescriptorSets(m_renderer->getDevice()->getHandle(), 1, writeSets, 0, nullptr);
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
        return m_renderer->getContext()->getPhysicalDevice().getRayTracingProperties().shaderGroupHandleSize;
    }

    void RayTracingMaterial::resetFrameCounter()
    {
        m_frameIdx = 0;
    }

    void RayTracingMaterial::createPipeline(Renderer* renderer)
    {
        VkShaderModule raygenSM = renderer->loadShaderModule("raytrace-rgen");
        VkShaderModule missSM   = renderer->loadShaderModule("raytrace-rmiss");
        VkShaderModule rchitSM  = renderer->loadShaderModule("raytrace-rchit");

        std::vector<VkPipelineShaderStageCreateInfo>     stages;
        std::vector<VkRayTracingShaderGroupCreateInfoNV> groups;

        // Ray gen group and stage
        VkRayTracingShaderGroupCreateInfoNV raygenGroup = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV };
        raygenGroup.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
        raygenGroup.anyHitShader       = VK_SHADER_UNUSED_NV;
        raygenGroup.closestHitShader   = VK_SHADER_UNUSED_NV;
        raygenGroup.generalShader      = 0;
        raygenGroup.intersectionShader = VK_SHADER_UNUSED_NV;
        groups.push_back(raygenGroup);

        VkPipelineShaderStageCreateInfo raygenInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        raygenInfo.stage  = VK_SHADER_STAGE_RAYGEN_BIT_NV;
        raygenInfo.module = raygenSM;
        raygenInfo.pName  = "main";
        stages.push_back(raygenInfo);

        // Ray miss group and stage
        VkRayTracingShaderGroupCreateInfoNV raymissGroup = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV };
        raymissGroup.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
        raymissGroup.anyHitShader       = VK_SHADER_UNUSED_NV;
        raymissGroup.closestHitShader   = VK_SHADER_UNUSED_NV;
        raymissGroup.generalShader      = 1;
        raymissGroup.intersectionShader = VK_SHADER_UNUSED_NV;
        groups.push_back(raymissGroup);

        VkPipelineShaderStageCreateInfo missInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        missInfo.stage  = VK_SHADER_STAGE_MISS_BIT_NV;
        missInfo.module = missSM;
        missInfo.pName  = "main";
        stages.push_back(missInfo);

        // Ray hit group and stage
        VkRayTracingShaderGroupCreateInfoNV rayHitGroup = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV };
        rayHitGroup.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
        rayHitGroup.anyHitShader       = VK_SHADER_UNUSED_NV;
        rayHitGroup.closestHitShader   = 2;
        rayHitGroup.generalShader      = VK_SHADER_UNUSED_NV;
        rayHitGroup.intersectionShader = VK_SHADER_UNUSED_NV;
        groups.push_back(rayHitGroup);

        VkPipelineShaderStageCreateInfo rayHitInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        rayHitInfo.stage  = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;
        rayHitInfo.module = rchitSM;
        rayHitInfo.pName  = "main";
        stages.push_back(rayHitInfo);

        // Pipeline Layout creation
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

        VkPushConstantRange pcRange;
        pcRange.offset = 0;
        pcRange.size = sizeof(uint32_t);
        pcRange.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_RAYGEN_BIT_NV;
        pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
        pipelineLayoutCreateInfo.pPushConstantRanges = &pcRange;
        pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(m_setLayouts.size());
        pipelineLayoutCreateInfo.pSetLayouts    = m_setLayouts.data();
        vkCreatePipelineLayout(renderer->getDevice()->getHandle(), &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);

        GET_DEVICE_PROC_NV(vkCreateRayTracingPipelines, renderer->getDevice());
        VkRayTracingPipelineCreateInfoNV raytracingCreateInfo = { VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV };
        raytracingCreateInfo.stageCount        = static_cast<uint32_t>(stages.size());
        raytracingCreateInfo.pStages           = stages.data();
        raytracingCreateInfo.groupCount        = static_cast<uint32_t>(groups.size());
        raytracingCreateInfo.pGroups           = groups.data();
        raytracingCreateInfo.maxRecursionDepth = 1;
        raytracingCreateInfo.layout            = m_pipelineLayout;
        vkCreateRayTracingPipelines(renderer->getDevice()->getHandle(), nullptr, 1, &raytracingCreateInfo, nullptr, &m_pipeline);

        GET_DEVICE_PROC_NV(vkGetRayTracingShaderGroupHandles, renderer->getDevice());
        uint32_t groupCount = static_cast<uint32_t>(groups.size());
        uint32_t handleSize = renderer->getContext()->getPhysicalDevice().getRayTracingProperties().shaderGroupHandleSize;
        VkDeviceSize shaderGroupBindingTableSize = groupCount * handleSize;
        std::vector<uint8_t> shaderHandleStorage(shaderGroupBindingTableSize);

        vkGetRayTracingShaderGroupHandles(renderer->getDevice()->getHandle(), m_pipeline, 0, groupCount,
            shaderGroupBindingTableSize, shaderHandleStorage.data());

        m_sbtBuffer = std::make_unique<VulkanBuffer>(renderer->getDevice(), shaderGroupBindingTableSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        renderer->enqueueResourceUpdate([&, this](VkCommandBuffer cmdBuffer)
        {
            vkCmdUpdateBuffer(cmdBuffer, m_sbtBuffer->getHandle(), 0, shaderHandleStorage.size(), shaderHandleStorage.data());
            vkDeviceWaitIdle(renderer->getDevice()->getHandle());
        });
        renderer->flushResourceUpdates(true);
    }
}
