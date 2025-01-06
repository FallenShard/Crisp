#include <Crisp/Vulkan/VulkanSynchronization.hpp>

#include <gmock/gmock.h>

namespace crisp {
namespace {
using ::testing::AllOf;
using ::testing::Field;

auto SynchronizationStageIs(const VkPipelineStageFlags2 stage, const VkAccessFlags2 access) {
    return AllOf(Field(&VulkanSynchronizationStage::stage, stage), Field(&VulkanSynchronizationStage::access, access));
}

auto SynchronizationScopeIs(
    const VkPipelineStageFlags2 srcStage,
    const VkAccessFlags2 srcAccess,
    const VkPipelineStageFlags2 dstStage,
    const VkAccessFlags2 dstAccess) {
    return AllOf(
        Field(&VulkanSynchronizationScope::srcStage, srcStage),
        Field(&VulkanSynchronizationScope::srcAccess, srcAccess),
        Field(&VulkanSynchronizationScope::dstStage, dstStage),
        Field(&VulkanSynchronizationScope::dstAccess, dstAccess));
}

TEST(VulkanSynchronizationTest, Stages) {
    EXPECT_THAT(
        kComputeRead, SynchronizationStageIs(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT));
}

TEST(VulkanSynchronizationTest, Scopes) {
    EXPECT_THAT(
        kComputeRead >> kComputeWrite,
        SynchronizationScopeIs(
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_WRITE_BIT));

    EXPECT_THAT(
        (kComputeRead | kVertexRead) >> kComputeWrite,
        SynchronizationScopeIs(
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_WRITE_BIT));
}

} // namespace
} // namespace crisp