#include <gtest/gtest.h>

#include <Crisp/Materials/PbrMaterial.hpp>

namespace crisp {
namespace {

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

} // namespace
} // namespace crisp
