
#include <Crisp/Core/Result.hpp>

#include <Crisp/Core/Test/ResultTestUtils.hpp>

#include <numeric>

namespace crisp::test
{
using ::testing::Not;

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
    EXPECT_THAT(Result<int>(5), HasValue(5));
}

TEST(ResultTest, BasicError)
{
    EXPECT_THAT(Result<int>{resultError("{}", "invalid path!")}, HasErrorWithMessageRegex("invalid path"));
}

TEST(ResultTest, ValueWithMoveOnly)
{
    Result<MoveOnlyType> result(MoveOnlyType{});
    EXPECT_THAT(result, Not(HasError()));

    const auto value = std::move(result).unwrap();
    EXPECT_EQ(*value.x, 10);
}
} // namespace crisp::test