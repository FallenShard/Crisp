#include <Test/VulkanTests/VulkanTest.hpp>

#include <Crisp/Core/ApplicationEnvironment.hpp>
#include <Crisp/Core/Window.hpp>
#include <Crisp/Vulkan/VulkanContext.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanQueue.hpp>
#include <Crisp/Vulkan/VulkanQueueConfiguration.hpp>

#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>

#include <Crisp/Common/Result.hpp>

#include <numeric>

using namespace crisp;

class PipelineLayoutBuilderTest : public VulkanTest
{
};

TEST_F(PipelineLayoutBuilderTest, BasicUsage)
{
    const auto& [deps, device] = createDevice();

    PipelineLayoutBuilder builder{};
    builder.defineDescriptorSet(
        0,
        false,
        {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT},
    });
    const auto bindings = builder.getDescriptorSetLayoutBindings();
    ASSERT_EQ(bindings.size(), 1);

    auto layoutHandles = builder.createDescriptorSetLayoutHandles(device->getHandle());
    ASSERT_EQ(layoutHandles.size(), 1);
    for (auto layout : layoutHandles)
        vkDestroyDescriptorSetLayout(device->getHandle(), layout, nullptr);

    /*auto descriptorSetAllocator = builder.createMinimalDescriptorSetAllocator(*device);
    descriptorSetAllocator->*/
}
