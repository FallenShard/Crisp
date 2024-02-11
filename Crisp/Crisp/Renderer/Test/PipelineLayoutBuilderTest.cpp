#include <Crisp/Vulkan/Test/VulkanTest.hpp>
#include <Crisp/Vulkan/VulkanBuffer.hpp>

#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>

namespace crisp {
namespace {
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
    const auto bindings = builder.getDescriptorSetLayoutBindings();
    ASSERT_EQ(bindings.size(), 1);

    auto layoutHandles = builder.createDescriptorSetLayoutHandles(device_->getHandle());
    ASSERT_EQ(layoutHandles.size(), 1);
    for (auto layout : layoutHandles) {
        vkDestroyDescriptorSetLayout(device_->getHandle(), layout, nullptr);
    }
}
} // namespace
} // namespace crisp