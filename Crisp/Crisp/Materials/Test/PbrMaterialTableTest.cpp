#include <Crisp/Materials/PbrMaterialTable.hpp>

#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

namespace crisp {
namespace {

using PbrMaterialTableTest = VulkanTest;

TEST_F(PbrMaterialTableTest, AssignsStableIndicesAndDrawParameters) {
    PbrMaterialTable table(*device_, 4);

    PbrParams first{};
    first.metallic = 0.25f;
    PbrParams second{};
    second.roughness = 0.75f;

    const auto firstHandle = table.add(first);
    const auto secondHandle = table.add(second);

    EXPECT_EQ(firstHandle.index, 0);
    EXPECT_EQ(secondHandle.index, 1);
    EXPECT_EQ(table.getMaterialCount(), 2);
    EXPECT_EQ(table.getCapacity(), 4);
    EXPECT_NE(table.getDeviceAddress(), 0);

    const auto drawParameters = table.createDrawParameters(secondHandle);
    EXPECT_EQ(drawParameters.materialTableAddress, table.getDeviceAddress());
    EXPECT_EQ(drawParameters.materialIndex, secondHandle.index);
}

TEST_F(PbrMaterialTableTest, UploadsPopulatedPrefixThroughStagingBelt) {
    PbrMaterialTable table(*device_, 4);

    PbrParams first{};
    first.albedo = glm::vec4(0.1f, 0.2f, 0.3f, 0.4f);
    first.samplerIndex = 3;
    PbrParams second{};
    second.albedoTex = 17;
    second.emissiveTex = 23;
    table.add(first);
    table.add(second);

    VulkanStagingBelt stagingBelt(*device_, 4096);
    stagingBelt.setRetirementValue(1);
    {
        ScopeCommandExecutor executor(*device_);
        table.updateDeviceBuffer(stagingBelt, executor.cmdEncoder);
    }

    const auto uploaded = toStdVec<PbrParams>(table.getDeviceBuffer());
    ASSERT_EQ(uploaded.size(), table.getCapacity());
    EXPECT_FLOAT_EQ(uploaded[0].albedo.x, first.albedo.x);
    EXPECT_FLOAT_EQ(uploaded[0].albedo.w, first.albedo.w);
    EXPECT_EQ(uploaded[0].samplerIndex, first.samplerIndex);
    EXPECT_EQ(uploaded[1].albedoTex, second.albedoTex);
    EXPECT_EQ(uploaded[1].emissiveTex, second.emissiveTex);
}

} // namespace
} // namespace crisp
