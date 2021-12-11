#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <memory>
#include "VulkanMemoryHeap.hpp"
#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
    class VulkanDevice;
    class Geometry;
    class VulkanImage;
    class VulkanImageView;
    class RayTracingMaterial;

    class VulkanRayTracer
    {
    public:
        VulkanRayTracer(VulkanDevice* device);
        ~VulkanRayTracer();

        void addTriangleGeometry(const Geometry& geometry, glm::mat4 transform);
        void createBottomLevelAccelerationStructures();
        void createTopLevelAccelerationStructure();


        void build(VkCommandBuffer cmdBuffer);
        void buildBottomLevelAccelerationStructure(VkCommandBuffer cmdBuffer, uint32_t index);
        void buildTopLevelAccelerationStructure(VkCommandBuffer cmdBuffer);

        VkAccelerationStructureNV getTopLevelAccelerationStructure() const;

        void createImage(uint32_t width, uint32_t height, uint32_t layerCount);
        VulkanImage* getImage() const;
        std::array<VkImageView, 2> getImageViewHandles() const;

        const std::vector<std::unique_ptr<VulkanImageView>>& getImageViews() const;

        void traceRays(VkCommandBuffer cmdBuffer, uint32_t virtualFrameIndex, const RayTracingMaterial& material) const;

    private:
        VulkanDevice* m_device;

        struct AccelerationStructure
        {
            VkAccelerationStructureNV handle;
            uint64_t rawHandle;

            VulkanMemoryHeap::Allocation allocation;
            VkAccelerationStructureInfoNV info;
        };

        std::vector<glm::mat4>                 m_blasTransforms;
        std::vector<std::vector<VkGeometryNV>> m_blasGeometries;
        std::vector<AccelerationStructure>     m_blasList;

        AccelerationStructure m_tlas;

        std::unique_ptr<VulkanImage> m_image;
        std::vector<std::unique_ptr<VulkanImageView>> m_imageViews;
    };
}