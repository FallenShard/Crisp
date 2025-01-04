#include <Crisp/Models/Ocean.hpp>

#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

#include <numeric>

namespace crisp {
namespace {
using OceanTest = VulkanTest;

using ::testing::IsNull;
using ::testing::Not;

TEST_F(OceanTest, PatchConstruction) {
    constexpr float kSize = 5.0;
    for (const int32_t N : {64, 128, 256}) {
        const TriangleMesh mesh = createGridMesh(kSize, N - 1);
        EXPECT_EQ(mesh.getVertexCount(), N * N);

        const auto boundingBox = mesh.getBoundingBox();
        EXPECT_NEAR(boundingBox.getExtents().x, kSize, 1e-8);
        EXPECT_NEAR(boundingBox.getExtents().y, 0.0, 1e-8);
        EXPECT_NEAR(boundingBox.getExtents().z, kSize, 1e-8);
    }
}

// TEST_F(OceanTest, PatchConstruction)
//{
//     constexpr int32_t N = 512;
//     constexpr float kSize = 5.0;
//     const std::vector<std::vector<VertexAttributeDescriptor>> vertexFormat = {
//         {VertexAttribute::Position}, {VertexAttribute::Normal}};
//
//     TriangleMesh mesh = createGridMesh(flatten(vertexFormat), kSize, N - 1);
//     EXPECT_EQ(mesh.getVertexCount(), N * N);
//
//     const auto boundingBox = mesh.getBoundingBox();
//     EXPECT_NEAR(boundingBox.getExtents().x, kSize, 1e-8);
//     EXPECT_NEAR(boundingBox.getExtents().y, 0.0, 1e-8);
//     EXPECT_NEAR(boundingBox.getExtents().z, kSize, 1e-8);
// }

TEST_F(OceanTest, VulkanBuffer) {
    constexpr VkDeviceSize elementCount = 25;
    constexpr VkDeviceSize size = sizeof(float) * elementCount;
    std::vector<float> data(elementCount);
    std::iota(data.begin(), data.end(), 0.0f);

    VulkanBuffer deviceBuffer(
        *device_,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    StagingVulkanBuffer stagingBuffer(*device_, size);
    const float* stagingPtr = stagingBuffer.getHostVisibleData<float>();
    stagingBuffer.updateFromHost(data);
    for (uint32_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(stagingPtr[i], data[i]);
    }

    StagingVulkanBuffer downloadBuffer(*device_, deviceBuffer.getSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    {
        ScopeCommandExecutor executor(*device_);
        const auto& cmdBuffer = executor.cmdBuffer;

        deviceBuffer.copyFrom(cmdBuffer.getHandle(), stagingBuffer);
        cmdBuffer.insertBufferMemoryBarrier(
            deviceBuffer.createDescriptorInfo(),
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT);

        downloadBuffer.copyFrom(cmdBuffer.getHandle(), deviceBuffer);
        cmdBuffer.insertBufferMemoryBarrier(
            downloadBuffer.createDescriptorInfo(),
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_ACCESS_HOST_READ_BIT);
    }

    const float* ptr = downloadBuffer.getHostVisibleData<float>();
    for (uint32_t i = 0; i < elementCount; ++i) {
        ASSERT_EQ(ptr[i], data[i]) << " not equal at index " << i;
    }
}

} // namespace
} // namespace crisp