#include <Crisp/Image/Image.hpp>

#include <gmock/gmock.h>

namespace crisp::test {
namespace {

TEST(MipLevelsTest, ComputesCorrectNumberOfMipLevels) {
    EXPECT_EQ(Image::getMipLevels(0, 0), 0);
    EXPECT_EQ(Image::getMipLevels(1, 1), 1);
    EXPECT_EQ(Image::getMipLevels(2, 2), 2);
    EXPECT_EQ(Image::getMipLevels(3, 3), 2);
    EXPECT_EQ(Image::getMipLevels(4, 4), 3);
    EXPECT_EQ(Image::getMipLevels(64, 64), 7);
    EXPECT_EQ(Image::getMipLevels(256, 4), 9);
    EXPECT_EQ(Image::getMipLevels(512, 4), 10);
    EXPECT_EQ(Image::getMipLevels(1024, 1024), 11);
    EXPECT_EQ(Image::getMipLevels(1023, 1023), 10);
    EXPECT_EQ(Image::getMipLevels(1021, 1023), 10);
}
} // namespace
} // namespace crisp::test