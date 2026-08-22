#include <gtest/gtest.h>

#include <Crisp/Materials/PbrMaterial.hpp>

namespace crisp {
namespace {

TEST(PbrMaterialTest, UsesOpenPbrOpaqueProfileDefaults) {
    const PbrMaterialParams params{};

    EXPECT_EQ(params.baseColor, glm::vec3(0.8f));
    EXPECT_FLOAT_EQ(params.baseWeight, 1.0f);
    EXPECT_EQ(params.specularColor, glm::vec3(1.0f));
    EXPECT_FLOAT_EQ(params.specularWeight, 1.0f);
    EXPECT_FLOAT_EQ(params.specularRoughness, 0.3f);
    EXPECT_FLOAT_EQ(params.specularIor, 1.5f);
    EXPECT_FLOAT_EQ(params.baseMetalness, 0.0f);
    EXPECT_FLOAT_EQ(params.baseDiffuseRoughness, 0.0f);
    EXPECT_EQ(params.emissionColor, glm::vec3(1.0f));
    EXPECT_FLOAT_EQ(params.emissionLuminance, 0.0f);
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
