#include <Crisp/Geometry/VertexLayout.hpp>

#include <gmock/gmock.h>

namespace crisp {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;

auto BindingIs(const uint32_t binding, const VkVertexInputRate inputRate, const uint32_t stride) {
    return AllOf(
        Field(&VkVertexInputBindingDescription::binding, binding),
        Field(&VkVertexInputBindingDescription::inputRate, inputRate),
        Field(&VkVertexInputBindingDescription::stride, stride));
}

auto AttributeIs(const uint32_t binding, const uint32_t location, const uint32_t offset, const VkFormat format) {
    return AllOf(
        Field(&VkVertexInputAttributeDescription::binding, binding),
        Field(&VkVertexInputAttributeDescription::format, format),
        Field(&VkVertexInputAttributeDescription::offset, offset),
        Field(&VkVertexInputAttributeDescription::location, location));
}

TEST(VertexLayoutTest, Simple) {

    VertexLayout layout{};
    layout.addBinding<VK_FORMAT_R32G32B32_SFLOAT>();
    layout.addBinding<VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT>();

    EXPECT_THAT(
        layout.bindings,
        ElementsAre(BindingIs(0, VK_VERTEX_INPUT_RATE_VERTEX, 12), BindingIs(1, VK_VERTEX_INPUT_RATE_VERTEX, 36)));

    EXPECT_THAT(
        layout.attributes,
        ElementsAre(
            AttributeIs(0, 0, 0, VK_FORMAT_R32G32B32_SFLOAT),
            AttributeIs(1, 1, 0, VK_FORMAT_R32G32B32_SFLOAT),
            AttributeIs(1, 2, 12, VK_FORMAT_R32G32_SFLOAT),
            AttributeIs(1, 3, 20, VK_FORMAT_R32G32B32A32_SFLOAT)));
}

} // namespace crisp