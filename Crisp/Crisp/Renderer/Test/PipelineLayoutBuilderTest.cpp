#include <Crisp/Vulkan/Rhi/VulkanBuffer.hpp>
#include <Crisp/Vulkan/Test/VulkanTest.hpp>


#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>

namespace crisp {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;

using PipelineLayoutBuilderTest = VulkanTest;

TEST_F(PipelineLayoutBuilderTest, BasicUsage) {
    PipelineLayoutBuilder builder{};
    builder.defineDescriptorSet(
        0,
        false,
        {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        });
    builder.defineDescriptorSet(
        1,
        true,
        {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        });
    builder.defineDescriptorSet(
        2,
        false,
        {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        });
    builder.setDescriptorBindless(2, 0, 10);

    auto layout = builder.create(*device_);
    ASSERT_EQ(layout->getDescriptorSetLayoutCount(), 3);
    EXPECT_EQ(layout->getDynamicBufferCount(), 4);

    EXPECT_THAT(layout->getDescriptorSetLayoutBindings(0), SizeIs(2));
    EXPECT_THAT(layout->getDescriptorSetBindlessBindings(0), IsEmpty());
    EXPECT_EQ(layout->isDescriptorSetBuffered(0), false);
    ASSERT_EQ(layout->getDynamicBufferCount(0), 1);
    EXPECT_EQ(layout->getDynamicBufferIndex(0, 0), 0);

    EXPECT_THAT(layout->getDescriptorSetLayoutBindings(1), SizeIs(4));
    EXPECT_THAT(layout->getDescriptorSetBindlessBindings(1), IsEmpty());
    EXPECT_EQ(layout->isDescriptorSetBuffered(1), true);
    ASSERT_EQ(layout->getDynamicBufferCount(1), 3);
    EXPECT_EQ(layout->getDynamicBufferIndex(1, 0), 1);
    EXPECT_EQ(layout->getDynamicBufferIndex(1, 2), 2);
    EXPECT_EQ(layout->getDynamicBufferIndex(1, 3), 3);

    EXPECT_THAT(layout->getDescriptorSetLayoutBindings(2), SizeIs(1));
    EXPECT_THAT(layout->getDescriptorSetBindlessBindings(2), ElementsAre(0));
    EXPECT_EQ(layout->isDescriptorSetBuffered(2), false);
    ASSERT_EQ(layout->getDynamicBufferCount(2), 0);
}
} // namespace
} // namespace crisp