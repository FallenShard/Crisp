#include <Crisp/Materials/PbrMaterialTable.hpp>

#include <Crisp/Vulkan/Rhi/Test/VulkanTest.hpp>

namespace crisp {
namespace {

using PbrMaterialTableTest = VulkanTest;

TEST_F(PbrMaterialTableTest, AssignsStableIndicesAndDrawParameters) {
    PbrMaterialTable table(*device_, 4);

    PbrMaterialParams first{};
    first.baseMetalness = 0.25f;
    PbrMaterialParams second{};
    second.specularRoughness = 0.75f;

    const auto firstHandle = table.add(first);
    const auto secondHandle = table.add(second);

    EXPECT_EQ(firstHandle.index, 0);
    EXPECT_EQ(secondHandle.index, 1);
    EXPECT_EQ(table.getMaterialCount(), 2);
    EXPECT_EQ(table.getCapacity(), 4);
    EXPECT_NE(table.getDeviceAddress(), 0);

    const auto drawParameters = table.createDrawParameters(secondHandle, PbrDrawFlag::RayTracedShadows);
    EXPECT_EQ(drawParameters.materialTableAddress, table.getDeviceAddress());
    EXPECT_EQ(drawParameters.materialIndex, secondHandle.index);
    EXPECT_EQ(drawParameters.flags, PbrDrawFlagFlags{PbrDrawFlag::RayTracedShadows}.getMask());
}

TEST_F(PbrMaterialTableTest, UploadsPopulatedPrefixThroughStagingBelt) {
    PbrMaterialTable table(*device_, 4);

    PbrMaterialParams first{};
    first.baseColor = glm::vec3(0.1f, 0.2f, 0.3f);
    first.geometryOpacity = 0.4f;
    first.samplerIndex = 3;
    PbrMaterialParams second{};
    second.baseColorTex = 17;
    second.emissionTex = 23;
    table.add(first);
    table.add(second);

    VulkanStagingBelt stagingBelt(*device_, 4096);
    stagingBelt.setRetirementValue(1);
    {
        ScopeCommandExecutor executor(*device_);
        table.updateDeviceBuffer(stagingBelt, executor.cmdEncoder);
    }

    const auto uploaded = toStdVec<PbrMaterialParams>(table.getDeviceBuffer());
    ASSERT_EQ(uploaded.size(), table.getCapacity());
    EXPECT_FLOAT_EQ(uploaded[0].baseColor.x, first.baseColor.x);
    EXPECT_FLOAT_EQ(uploaded[0].geometryOpacity, first.geometryOpacity);
    EXPECT_EQ(uploaded[0].samplerIndex, first.samplerIndex);
    EXPECT_EQ(uploaded[1].baseColorTex, second.baseColorTex);
    EXPECT_EQ(uploaded[1].emissionTex, second.emissionTex);
}

TEST_F(PbrMaterialTableTest, UpdatesAnExistingMaterialWithoutChangingItsHandle) {
    PbrMaterialTable table(*device_, 2);
    const auto handle = table.add(PbrMaterialParams{});

    PbrMaterialParams updated{};
    updated.baseColor = glm::vec3(0.2f, 0.4f, 0.6f);
    updated.geometryOpacity = 0.8f;
    updated.baseMetalness = 0.75f;
    updated.specularRoughness = 0.15f;
    table.update(handle, updated);

    VulkanStagingBelt stagingBelt(*device_, 4096);
    stagingBelt.setRetirementValue(1);
    {
        ScopeCommandExecutor executor(*device_);
        table.updateDeviceBuffer(stagingBelt, executor.cmdEncoder);
    }

    const auto uploaded = toStdVec<PbrMaterialParams>(table.getDeviceBuffer());
    EXPECT_EQ(table.getMaterialCount(), 1);
    EXPECT_EQ(table.createDrawParameters(handle).materialIndex, handle.index);
    EXPECT_EQ(uploaded[handle.index].baseColor, updated.baseColor);
    EXPECT_FLOAT_EQ(uploaded[handle.index].geometryOpacity, updated.geometryOpacity);
    EXPECT_FLOAT_EQ(uploaded[handle.index].baseMetalness, updated.baseMetalness);
    EXPECT_FLOAT_EQ(uploaded[handle.index].specularRoughness, updated.specularRoughness);
}

} // namespace
} // namespace crisp
