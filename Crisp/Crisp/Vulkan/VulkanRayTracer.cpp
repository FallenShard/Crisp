#include "VulkanRayTracer.hpp"

#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Vulkan/RayTracingMaterial.hpp>

#define GET_DEVICE_PROC_NV(procVar) getDeviceProc(procVar, device, #procVar "NV")

namespace crisp
{
    namespace
    {
        PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructure;
        PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirements;
        PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemory;
        PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandle;
        PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructure;
        PFN_vkCmdTraceRaysNV vkCmdTraceRays;


        struct VkGeometryInstanceNV
        {
            float          transform[12];
            uint32_t       instanceId : 24;
            uint32_t       mask : 8;
            uint32_t       hitGroupId : 24;
            uint32_t       flags : 8;
            uint64_t       accelerationStructureHandle;
        };

        template <typename T>
        void getDeviceProc(T& procRef, VulkanDevice* device, const char* procName)
        {
            procRef = reinterpret_cast<T>(vkGetDeviceProcAddr(device->getHandle(), procName));
        }

        void initializeExtensionFunctions(VulkanDevice* device)
        {
            GET_DEVICE_PROC_NV(vkCreateAccelerationStructure);
            GET_DEVICE_PROC_NV(vkGetAccelerationStructureMemoryRequirements);
            GET_DEVICE_PROC_NV(vkBindAccelerationStructureMemory);
            GET_DEVICE_PROC_NV(vkGetAccelerationStructureHandle);
            GET_DEVICE_PROC_NV(vkCmdBuildAccelerationStructure);
            GET_DEVICE_PROC_NV(vkCmdTraceRays);
        }
    }

    VulkanRayTracer::VulkanRayTracer(VulkanDevice* device)
        : m_device(device)
    {
        initializeExtensionFunctions(device);

        //m_rayTracer->addTriangleGeometry(*m_geometries.at("cubeShadow"), m_renderNodes.at("cube")->transformPack->M);
        //m_rayTracer->addTriangleGeometry(*m_geometries.at("floorPos"), m_renderNodes.at("floor")->transformPack->M);

        ////m_rayTracer->addTriangleGeometry(*m_geometries.at("cubeShadow"), glm::translate(glm::vec3(-2.0f, 0.50f, -3.0f)) * glm::rotate(glm::radians(60.0f), glm::vec3(0.0f, 1.0f, 0.0f)));

        //// Build the blas
        //m_renderer->enqueueResourceUpdate([&](VkCommandBuffer cmdBuffer)
        //{
        //    m_rayTracer->build(cmdBuffer);
        //});
        //m_renderer->flushResourceUpdates(true);

        //m_renderer->getDevice()->flushDescriptorUpdates();

        //auto screenSize = m_renderer->getSwapChainExtent();
        //m_rayTracer->createImage(screenSize.width, screenSize.height, RendererConfig::VirtualFrameCount);
        //m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
        //{
        //    m_rayTracer->getImage()->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_GENERAL,
        //        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV);
        //});

        //m_rayTracingMaterial = std::make_unique<RayTracingMaterial>(m_renderer, m_rayTracer->getTopLevelAccelerationStructure(), m_rayTracer->getImageViewHandles(), *m_uniformBuffers["camera"]);

        //m_renderer->setSceneImageViews(m_rayTracer->getImageViews());
    }

    void VulkanRayTracer::addTriangleGeometry(const Geometry& geometry, glm::mat4 transform)
    {
        VkGeometryNV vkGeometry = { VK_STRUCTURE_TYPE_GEOMETRY_NV };
        vkGeometry.flags        = VK_GEOMETRY_OPAQUE_BIT_NV;
        vkGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
        vkGeometry.geometry     = {};
        vkGeometry.geometry.triangles = { VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV };
        vkGeometry.geometry.triangles.vertexData   = geometry.getVertexBuffer()->getHandle();
        vkGeometry.geometry.triangles.vertexOffset = 0;
        vkGeometry.geometry.triangles.vertexCount  = geometry.getVertexCount();
        vkGeometry.geometry.triangles.vertexStride = sizeof(glm::vec4) * 2;
        vkGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        vkGeometry.geometry.triangles.indexData    = geometry.getIndexBuffer()->getHandle();
        vkGeometry.geometry.triangles.indexOffset  = 0;
        vkGeometry.geometry.triangles.indexCount   = geometry.getIndexCount();
        vkGeometry.geometry.triangles.indexType    = VK_INDEX_TYPE_UINT32;
        vkGeometry.geometry.aabbs = { VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV };

        m_blasGeometries.push_back({});
        m_blasGeometries.back().push_back(vkGeometry);
        m_blasTransforms.push_back(transform);
    }

    VulkanRayTracer::~VulkanRayTracer()
    {
    }

    void VulkanRayTracer::createBottomLevelAccelerationStructures()
    {
        for (uint32_t i = 0; i < m_blasGeometries.size(); ++i)
        {
            // Create the acceleration structure
            VkAccelerationStructureCreateInfoNV asCreateInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV };
            asCreateInfo.info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV };
            asCreateInfo.info.geometryCount = static_cast<uint32_t>(m_blasGeometries[i].size());
            asCreateInfo.info.pGeometries   = m_blasGeometries[i].data();
            asCreateInfo.info.instanceCount = 0;
            asCreateInfo.info.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV;
            asCreateInfo.info.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;

            AccelerationStructure accelStruct;
            accelStruct.info = asCreateInfo.info;
            vkCreateAccelerationStructure(m_device->getHandle(), &asCreateInfo, nullptr, &accelStruct.handle);

            // Allocate its memory
            VkAccelerationStructureMemoryRequirementsInfoNV memReqInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV };
            memReqInfo.accelerationStructure = accelStruct.handle;
            memReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;

            VkMemoryRequirements2 memReq2 = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
            vkGetAccelerationStructureMemoryRequirements(m_device->getHandle(), &memReqInfo, &memReq2);

            auto* memHeap = m_device->getMemoryAllocator()->getHeapFromMemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memReq2.memoryRequirements.memoryTypeBits);
            accelStruct.memoryChunk = memHeap->allocate(memReq2.memoryRequirements.size, memReq2.memoryRequirements.alignment);

            // Bind the object and memory
            VkBindAccelerationStructureMemoryInfoNV bindInfo = { VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV };
            bindInfo.accelerationStructure = accelStruct.handle;
            bindInfo.deviceIndexCount      = 0;
            bindInfo.pDeviceIndices        = nullptr;
            bindInfo.memory                = accelStruct.memoryChunk.allocationBlock->memory;
            bindInfo.memoryOffset          = accelStruct.memoryChunk.offset;
            vkBindAccelerationStructureMemory(m_device->getHandle(), 1, &bindInfo);

            // Get its raw address handle
            vkGetAccelerationStructureHandle(m_device->getHandle(), accelStruct.handle, sizeof(accelStruct.rawHandle), &accelStruct.rawHandle);

            m_blasList.push_back(accelStruct);
        }
    }

    void VulkanRayTracer::buildBottomLevelAccelerationStructure(VkCommandBuffer cmdBuffer, uint32_t index)
    {
        AccelerationStructure& blas = m_blasList[index];

        VkAccelerationStructureMemoryRequirementsInfoNV memReqInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV };
        memReqInfo.accelerationStructure = blas.handle;
        memReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;

        VkMemoryRequirements2 scratchBufferMemReq = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
        vkGetAccelerationStructureMemoryRequirements(m_device->getHandle(), &memReqInfo, &scratchBufferMemReq);

        auto scratchBuffer = std::make_unique<VulkanBuffer>(m_device, scratchBufferMemReq.memoryRequirements.size, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkCmdBuildAccelerationStructure(cmdBuffer, &blas.info, nullptr, 0, VK_FALSE, blas.handle, nullptr, scratchBuffer->getHandle(), 0);
    }

    void VulkanRayTracer::createTopLevelAccelerationStructure()
    {
        // Create
        VkAccelerationStructureCreateInfoNV tlasCreate = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV };
        tlasCreate.info               = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV };
        tlasCreate.info.instanceCount = static_cast<uint32_t>(m_blasList.size());
        tlasCreate.info.geometryCount = 0;
        tlasCreate.info.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV;
        tlasCreate.info.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;

        m_tlas.info = tlasCreate.info;
        vkCreateAccelerationStructure(m_device->getHandle(), &tlasCreate, nullptr, &m_tlas.handle);

        // Allocate memory
        VkAccelerationStructureMemoryRequirementsInfoNV memReqInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV };
        memReqInfo.accelerationStructure = m_tlas.handle;
        memReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;

        VkMemoryRequirements2 memReq = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
        vkGetAccelerationStructureMemoryRequirements(m_device->getHandle(), &memReqInfo, &memReq);

        auto* memHeap = m_device->getMemoryAllocator()->getHeapFromMemProps(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memReq.memoryRequirements.memoryTypeBits);
        m_tlas.memoryChunk = memHeap->allocate(memReq.memoryRequirements.size, memReq.memoryRequirements.alignment);

        // Bind
        VkBindAccelerationStructureMemoryInfoNV tlasBindInfo = { VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV };
        tlasBindInfo.accelerationStructure = m_tlas.handle;
        tlasBindInfo.deviceIndexCount      = 0;
        tlasBindInfo.pDeviceIndices        = nullptr;
        tlasBindInfo.memory                = m_tlas.memoryChunk.allocationBlock->memory;
        tlasBindInfo.memoryOffset          = m_tlas.memoryChunk.offset;
        vkBindAccelerationStructureMemory(m_device->getHandle(), 1, &tlasBindInfo);

        // Get its raw address handle
        vkGetAccelerationStructureHandle(m_device->getHandle(), m_tlas.handle, sizeof(m_tlas.rawHandle), &m_tlas.rawHandle);
    }

    void VulkanRayTracer::build(VkCommandBuffer cmdBuffer)
    {
        createBottomLevelAccelerationStructures();
        createTopLevelAccelerationStructure();

        for (uint32_t i = 0; i < m_blasList.size(); ++i)
            buildBottomLevelAccelerationStructure(cmdBuffer, i);

        buildTopLevelAccelerationStructure(cmdBuffer);
    }

    void VulkanRayTracer::buildTopLevelAccelerationStructure(VkCommandBuffer cmdBuffer)
    {
        VkAccelerationStructureMemoryRequirementsInfoNV memReqInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV };
        memReqInfo.accelerationStructure = m_tlas.handle;
        memReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;

        VkMemoryRequirements2 scratchBufferMemReq = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
        vkGetAccelerationStructureMemoryRequirements(m_device->getHandle(), &memReqInfo, &scratchBufferMemReq);

        auto scratchBuffer = std::make_unique<VulkanBuffer>(m_device, scratchBufferMemReq.memoryRequirements.size, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        std::vector<VkGeometryInstanceNV> instances;
        instances.reserve(m_blasList.size());
        for (uint32_t i = 0; i < m_blasList.size(); ++i)
        {
            VkGeometryInstanceNV instance = {};
            memcpy(instance.transform, glm::value_ptr(glm::transpose(m_blasTransforms[i])), 12 * sizeof(float));
            instance.instanceId                  = i;
            instance.mask                        = 0xFF;
            instance.hitGroupId                  = 0;
            instance.flags                       = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
            instance.accelerationStructureHandle = m_blasList[i].rawHandle;
            instances.push_back(instance);
        }

        auto instanceBuffer = std::make_unique<VulkanBuffer>(m_device, sizeof(VkGeometryInstanceNV), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkCmdUpdateBuffer(cmdBuffer, instanceBuffer->getHandle(), 0, instances.size() * sizeof(VkGeometryInstanceNV), instances.data());
        vkDeviceWaitIdle(m_device->getHandle());

        vkCmdBuildAccelerationStructure(cmdBuffer, &m_tlas.info, instanceBuffer->getHandle(), 0, VK_FALSE, m_tlas.handle, nullptr, scratchBuffer->getHandle(), 0);
    }

    VkAccelerationStructureNV VulkanRayTracer::getTopLevelAccelerationStructure() const
    {
        return m_tlas.handle;
    }

    void VulkanRayTracer::createImage(uint32_t width, uint32_t height, uint32_t layerCount)
    {
        VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        createInfo.flags         = 0;
        createInfo.imageType     = VK_IMAGE_TYPE_2D;
        createInfo.format        = VK_FORMAT_R32G32B32A32_SFLOAT;
        createInfo.extent        = VkExtent3D{ width, height, 1 };
        createInfo.mipLevels     = 1;
        createInfo.arrayLayers   = 1;//layerCount;
        createInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        createInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        createInfo.usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_image = std::make_unique<VulkanImage>(m_device, createInfo, VK_IMAGE_ASPECT_COLOR_BIT);

        for (uint32_t i = 0; i < layerCount; ++i)
            m_imageViews.emplace_back(m_image->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 1));
    }

    VulkanImage* VulkanRayTracer::getImage() const
    {
        return m_image.get();
    }

    std::array<VkImageView, 2> VulkanRayTracer::getImageViewHandles() const
    {
        return { m_imageViews[0]->getHandle(), m_imageViews[1]->getHandle() };
    }

    const std::vector<std::unique_ptr<VulkanImageView>>& VulkanRayTracer::getImageViews() const
    {
        return m_imageViews;
    }

    void VulkanRayTracer::traceRays(VkCommandBuffer cmdBuffer, uint32_t virtualFrameIndex, const RayTracingMaterial& material) const
    {
        m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_GENERAL, 0, 1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV);

        material.bind(cmdBuffer, virtualFrameIndex);
        VkDeviceSize progSize       = material.getShaderGroupHandleSize();
        VkDeviceSize rayGenOffset   = 0;
        VkDeviceSize missOffset     = 1 * progSize;
        VkDeviceSize missStride     = progSize;
        VkDeviceSize hitGroupOffset = 2 * progSize;
        VkDeviceSize hitGroupStride = progSize;

        auto extent = material.getExtent();

        auto sbtBuffer = material.getShaderBindingTableBuffer();
        vkCmdTraceRays(cmdBuffer,
            sbtBuffer->getHandle(), rayGenOffset,
            sbtBuffer->getHandle(), missOffset, missStride,
            sbtBuffer->getHandle(), hitGroupOffset, hitGroupStride,
            sbtBuffer->getHandle(), 0, 0,
            extent.width, extent.height, 1);



        //VkImageMemoryBarrier barrier = {};
        //barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        //barrier.image = m_image->getHandle();
        //barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        //barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        //barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        //barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        //barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        //barrier.subresourceRange.baseArrayLayer = virtualFrameIndex;
        //barrier.subresourceRange.layerCount = 1;
        //barrier.subresourceRange.baseMipLevel = 0;
        //barrier.subresourceRange.levelCount = 1;
        //vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        //
        //m_renderer->waitIdle();

        m_image->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }
}