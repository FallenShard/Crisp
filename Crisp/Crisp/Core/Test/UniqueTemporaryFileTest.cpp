#include <Crisp/Core/UniqueTemporaryFile.hpp>

#include <filesystem>
#include <stdexcept>
#include <utility>

#include <gtest/gtest.h>

namespace crisp {
namespace {

TEST(UniqueTemporaryFileTest, CreatesUniqueFilesWithRequestedNames) {
    UniqueTemporaryFile first;
    UniqueTemporaryFile second("dot", "render-graph-", "-debug");

    EXPECT_NE(first.getPath(), second.getPath());
    EXPECT_EQ(first.getPath().extension(), "");
    EXPECT_EQ(second.getPath().extension(), ".dot");
    EXPECT_TRUE(second.getPath().stem().string().starts_with("render-graph-"));
    EXPECT_TRUE(second.getPath().stem().string().ends_with("-debug"));
    EXPECT_TRUE(std::filesystem::is_regular_file(first.getPath()));
    EXPECT_TRUE(std::filesystem::is_regular_file(second.getPath()));
}

TEST(UniqueTemporaryFileTest, RemovesFileOnScopeExit) {
    std::filesystem::path path;
    {
        UniqueTemporaryFile temporaryFile(".txt");
        path = temporaryFile.getPath();
        ASSERT_TRUE(std::filesystem::exists(path));
    }

    EXPECT_FALSE(std::filesystem::exists(path));
    EXPECT_FALSE(std::filesystem::exists(path.parent_path()));
}

TEST(UniqueTemporaryFileTest, TransfersCleanupOwnershipWhenMoved) {
    std::filesystem::path path;
    {
        UniqueTemporaryFile source("bin");
        path = source.getPath();
        UniqueTemporaryFile destination(std::move(source));

        EXPECT_TRUE(source.getPath().empty());
        EXPECT_EQ(destination.getPath(), path);
        EXPECT_TRUE(std::filesystem::exists(path));
    }

    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST(UniqueTemporaryFileTest, RejectsPathsInFilenameComponents) {
    EXPECT_THROW(UniqueTemporaryFile("nested/file.txt"), std::invalid_argument);
    EXPECT_THROW(UniqueTemporaryFile("txt", "nested/"), std::invalid_argument);
    EXPECT_THROW(UniqueTemporaryFile("txt", {}, "/nested"), std::invalid_argument);
}

} // namespace
} // namespace crisp
