#include <gtest/gtest.h>

#include <Crisp/Materials/PbrMaterial.hpp>

namespace crisp {
namespace {

TEST(PbrMaterialTest, UsesOpenPbrOpaqueProfileDefaults) {
    const PbrMaterialParams params{};

    EXPECT_EQ(params.surface.baseColor, glm::vec3(0.8f));
    EXPECT_FLOAT_EQ(params.surface.baseWeight, 1.0f);
    EXPECT_EQ(params.surface.specularColor, glm::vec3(1.0f));
    EXPECT_FLOAT_EQ(params.surface.specularWeight, 1.0f);
    EXPECT_FLOAT_EQ(params.surface.specularRoughness, 0.3f);
    EXPECT_FLOAT_EQ(params.surface.specularIor, 1.5f);
    EXPECT_FLOAT_EQ(params.surface.baseMetalness, 0.0f);
    EXPECT_FLOAT_EQ(params.surface.baseDiffuseRoughness, 0.0f);
    EXPECT_EQ(params.surface.emissionColor, glm::vec3(1.0f));
    EXPECT_FLOAT_EQ(params.surface.emissionLuminance, 0.0f);
    EXPECT_FLOAT_EQ(params.geometryOpacity, 1.0f);
}

TEST(PbrMaterialTest, PacksOrmChannels) {
    const Image occlusion{{17}, 1, 1, 1, 1};
    const Image roughness{{33}, 1, 1, 1, 1};
    const Image metallic{{65}, 1, 1, 1, 1};

    const Image orm = createPbrOrmMap({
        .occlusion = &occlusion,
        .roughness = &roughness,
        .metallic = &metallic,
    });

    ASSERT_EQ(orm.getChannelCount(), 4);
    ASSERT_EQ(orm.getByteSize(), 4);
    EXPECT_EQ(orm.getData()[0], 17);
    EXPECT_EQ(orm.getData()[1], 33);
    EXPECT_EQ(orm.getData()[2], 65);
    EXPECT_EQ(orm.getData()[3], 255);
}

TEST(PbrMaterialTest, UsesGltfDefaultsForMissingOrmChannels) {
    const Image roughness{{91}, 1, 1, 1, 1};

    const Image orm = createPbrOrmMap({.roughness = &roughness});

    ASSERT_EQ(orm.getByteSize(), 4);
    EXPECT_EQ(orm.getData()[0], 255);
    EXPECT_EQ(orm.getData()[1], 91);
    EXPECT_EQ(orm.getData()[2], 0);
    EXPECT_EQ(orm.getData()[3], 255);
}

TEST(PbrMaterialTest, UsesWhiteAsTheDefaultEmissiveMultiplier) {
    const Image emissive = createDefaultEmissiveMap();

    ASSERT_EQ(emissive.getByteSize(), 4);
    EXPECT_EQ(emissive.getData()[0], 255);
    EXPECT_EQ(emissive.getData()[1], 255);
    EXPECT_EQ(emissive.getData()[2], 255);
    EXPECT_EQ(emissive.getData()[3], 255);
}

} // namespace
} // namespace crisp
