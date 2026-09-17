#include <Crisp/Vulkan/PipelineBuilder.hpp>

#include <array>
#include <set>

#include <gmock/gmock.h>

namespace crisp {
namespace {
using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr std::array kAllDynamicStates{
    PipelineDynamicState::Viewport,
    PipelineDynamicState::Scissor,
    PipelineDynamicState::LineWidth,
    PipelineDynamicState::DepthBias,
    PipelineDynamicState::BlendConstants,
    PipelineDynamicState::DepthBounds,
    PipelineDynamicState::StencilCompareMask,
    PipelineDynamicState::StencilWriteMask,
    PipelineDynamicState::StencilReference,
    PipelineDynamicState::CullMode,
    PipelineDynamicState::FrontFace,
    PipelineDynamicState::PrimitiveTopology,
    PipelineDynamicState::DepthTestEnable,
    PipelineDynamicState::DepthWriteEnable,
    PipelineDynamicState::DepthCompareOp,
    PipelineDynamicState::DepthBoundsTestEnable,
    PipelineDynamicState::StencilTestEnable,
    PipelineDynamicState::StencilOp,
    PipelineDynamicState::RasterizerDiscardEnable,
    PipelineDynamicState::DepthBiasEnable,
    PipelineDynamicState::PrimitiveRestartEnable,
};

TEST(PipelineBuilderTest, NoDynamicStatesByDefault) {
    const PipelineBuilder builder{};
    EXPECT_TRUE(builder.getDynamicStateFlags().empty());
    EXPECT_THAT(toVkDynamicStates(builder.getDynamicStateFlags()), IsEmpty());
}

TEST(PipelineBuilderTest, AddedDynamicStatesAccumulate) {
    PipelineBuilder builder{};
    builder.addDynamicState(PipelineDynamicState::Viewport).addDynamicState(PipelineDynamicState::CullMode);

    const auto flags = builder.getDynamicStateFlags();
    EXPECT_TRUE(flags.contains(PipelineDynamicState::Viewport));
    EXPECT_TRUE(flags.contains(PipelineDynamicState::CullMode));
    EXPECT_FALSE(flags.contains(PipelineDynamicState::Scissor));

    EXPECT_THAT(toVkDynamicStates(flags), ElementsAre(VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_CULL_MODE));
}

TEST(PipelineBuilderTest, AddingTheSameDynamicStateTwiceYieldsOneEntry) {
    PipelineBuilder builder{};
    builder.addDynamicState(PipelineDynamicState::Scissor).addDynamicState(PipelineDynamicState::Scissor);

    EXPECT_THAT(toVkDynamicStates(builder.getDynamicStateFlags()), ElementsAre(VK_DYNAMIC_STATE_SCISSOR));
}

TEST(PipelineBuilderTest, AddDynamicStatesAcceptsAFlagSet) {
    PipelineBuilder builder{};
    builder.addDynamicStates(PipelineDynamicState::Viewport | PipelineDynamicState::Scissor);

    EXPECT_THAT(
        toVkDynamicStates(builder.getDynamicStateFlags()),
        ElementsAre(VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR));
}

// Every enumerator must reach the table; a missing entry would silently drop the state at pipeline creation.
TEST(PipelineBuilderTest, EveryDynamicStateMapsToADistinctVulkanState) {
    PipelineDynamicStateFlags allFlags;
    for (const auto state : kAllDynamicStates) {
        allFlags |= state;
    }

    const auto vkStates = toVkDynamicStates(allFlags);
    EXPECT_EQ(vkStates.size(), kAllDynamicStates.size());
    EXPECT_THAT(std::set(vkStates.begin(), vkStates.end()), ::testing::SizeIs(kAllDynamicStates.size()));
}

TEST(PipelineBuilderTest, DynamicStateNamesRoundTrip) {
    for (const auto state : kAllDynamicStates) {
        EXPECT_EQ(parsePipelineDynamicState(toString(state)), state) << "for " << toString(state);
    }
}

TEST(PipelineBuilderTest, UnknownDynamicStateNameParsesToNone) {
    EXPECT_EQ(parsePipelineDynamicState("polygonMode"), PipelineDynamicState::None);
    EXPECT_EQ(parsePipelineDynamicState(""), PipelineDynamicState::None);
}
} // namespace
} // namespace crisp
