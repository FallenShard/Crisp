#include <Crisp/Io/FileUtils.hpp>

#include <array>
#include <vector>

#include <Crisp/Core/UniqueTemporaryFile.hpp>

#include <gtest/gtest.h>

namespace crisp::test {
namespace {

TEST(FileUtilsTest, BinaryFileRoundTripPreservesBytes) {
    const UniqueTemporaryFile file("bin");
    constexpr std::array<char, 5> kData{'\0', '\x01', '\x7f', static_cast<char>(0x80), static_cast<char>(0xff)};

    writeBinaryFile(file.getPath(), std::as_bytes(std::span(kData))).unwrap();
    const auto result = readBinaryFile(file.getPath());

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(*result, std::vector<char>(kData.begin(), kData.end()));
}

TEST(FileUtilsTest, WriteBinaryFileTruncatesExistingFile) {
    const UniqueTemporaryFile file("bin");
    constexpr std::array<char, 4> kInitialData{'a', 'b', 'c', 'd'};
    constexpr std::array<char, 2> kReplacementData{'x', 'y'};

    writeBinaryFile(file.getPath(), std::as_bytes(std::span(kInitialData))).unwrap();
    writeBinaryFile(file.getPath(), std::as_bytes(std::span(kReplacementData))).unwrap();
    const auto result = readBinaryFile(file.getPath());

    ASSERT_TRUE(result.hasValue());
    EXPECT_EQ(*result, std::vector<char>(kReplacementData.begin(), kReplacementData.end()));
}

} // namespace
} // namespace crisp::test
