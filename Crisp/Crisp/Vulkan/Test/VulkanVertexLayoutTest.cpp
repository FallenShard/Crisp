#include <Crisp/Vulkan/VulkanVertexLayout.hpp>

#include <gmock/gmock.h>

namespace crisp {
namespace {
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;

auto InputBindingIs(const uint32_t binding, const uint32_t stride, const VkVertexInputRate inputRate) {
    return AllOf(
        Field(&VkVertexInputBindingDescription::binding, binding),
        Field(&VkVertexInputBindingDescription::stride, stride),
        Field(&VkVertexInputBindingDescription::inputRate, inputRate));
}

auto AttributeIs(const uint32_t location, const uint32_t binding, const uint32_t offset, const VkFormat format) {
    return AllOf(
        Field(&VkVertexInputAttributeDescription::location, location),
        Field(&VkVertexInputAttributeDescription::binding, binding),
        Field(&VkVertexInputAttributeDescription::offset, offset),
        Field(&VkVertexInputAttributeDescription::format, format));
}

TEST(VulkanVertexLayoutTest, AddBindings) {
    VulkanVertexLayout layout{};

    layout.addBinding<VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT>();
    layout.addBinding<VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT>();

    EXPECT_THAT(
        layout.bindings,
        ElementsAre(
            InputBindingIs(0, 20, VK_VERTEX_INPUT_RATE_VERTEX), InputBindingIs(1, 20, VK_VERTEX_INPUT_RATE_VERTEX)));
    EXPECT_THAT(
        layout.attributes,
        ElementsAre(
            AttributeIs(0, 0, 0, VK_FORMAT_R32G32B32_SFLOAT),
            AttributeIs(1, 0, 12, VK_FORMAT_R32G32_SFLOAT),
            AttributeIs(2, 1, 0, VK_FORMAT_R32_SFLOAT),
            AttributeIs(3, 1, 4, VK_FORMAT_R32G32B32A32_SFLOAT)));
}

TEST(VulkanVertexLayoutTest, Equality) {
    VulkanVertexLayout layout{};
    layout.addBinding<VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT>();
    layout.addBinding<VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT>();

    const auto layout2 = layout;
    EXPECT_EQ(layout, layout2);
}

TEST(VulkanVertexLayoutTest, SubsetOf) {
    VulkanVertexLayout layout{};
    layout.addBinding<VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT>();
    layout.addBinding<VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT>();

    VulkanVertexLayout layout2{};
    layout2.addBinding<VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT>();

    VulkanVertexLayout layout3{};
    layout3.addBinding<VK_FORMAT_R32G32B32_SFLOAT>();

    VulkanVertexLayout layout4{};
    layout4.addBinding<VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT>();

    EXPECT_TRUE(layout2.isSubsetOf(layout));
    EXPECT_TRUE(VulkanVertexLayout{}.isSubsetOf(layout2));
    EXPECT_TRUE(VulkanVertexLayout{}.isSubsetOf(layout));

    EXPECT_FALSE(layout3.isSubsetOf(layout));
    EXPECT_FALSE(layout.isSubsetOf(layout3));

    EXPECT_FALSE(layout4.isSubsetOf(layout));
}

} // namespace
} // namespace crisp