
#include <Crisp/Core/Result.hpp>

#include <gtest/gtest.h>

#include <numeric>

using namespace crisp;

struct MoveOnlyType
{
    MoveOnlyType() = default;
    ~MoveOnlyType() = default;

    MoveOnlyType(const MoveOnlyType&) = delete;
    MoveOnlyType& operator=(const MoveOnlyType&) = delete;

    MoveOnlyType(MoveOnlyType&&) noexcept = default;
    MoveOnlyType& operator=(MoveOnlyType&&) noexcept = default;

    std::unique_ptr<int> x{std::make_unique<int>(10)};
};

TEST(ResultTest, Basic)
{
    Result<int> result(5);

    EXPECT_TRUE(result.hasValue());
    EXPECT_EQ(std::move(result).unwrap(), 5);
}

TEST(ResultTest, BasicError)
{
    const Result<int> errorResult = resultError("{}", "invalid path!");

    EXPECT_FALSE(errorResult.hasValue());
    EXPECT_EQ(errorResult.getError(), "invalid path!");
}

TEST(ResultTest, ValueWithMoveOnly)
{
    Result<MoveOnlyType> result(MoveOnlyType{});

    EXPECT_TRUE(result.hasValue());

    const auto value = std::move(result).unwrap();
    EXPECT_EQ(*value.x, 10);
}
