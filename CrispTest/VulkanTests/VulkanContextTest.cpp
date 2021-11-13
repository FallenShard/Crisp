#include <gtest/gtest.h>


#include <Crisp/Vulkan/VulkanContext.hpp>


TEST(VulkanContextTest, Basic)
{
    crisp::VulkanContext context(nullptr, {}, false);

    ASSERT_EQ(true, true);
}
