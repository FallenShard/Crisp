
#include <Crisp/Core/Result.hpp>

#include <Crisp/Core/Test/ResultTestUtils.hpp>

namespace crisp {
using ::testing::Not;

namespace {
struct MoveOnlyType {
    MoveOnlyType() = default;
    ~MoveOnlyType() = default;

    MoveOnlyType(const MoveOnlyType&) = delete;
    MoveOnlyType& operator=(const MoveOnlyType&) = delete;

    MoveOnlyType(MoveOnlyType&&) noexcept = default;
    MoveOnlyType& operator=(MoveOnlyType&&) noexcept = default;

    std::unique_ptr<int> x{std::make_unique<int>(10)};
};

Result<> createFailure() {
    return resultError("original failure");
}

Result<int> propagateFailure() {
    CRISP_TRY(createFailure());
    return 42;
}

Result<int> propagateFailureWithContext() {
    CRISP_TRY(createFailure(), "contextual failure: {}", 17);
    return 42;
}

Result<int> continueAfterSuccess() {
    CRISP_TRY(Result<>{}, "unexpected failure");
    return 42;
}

Result<int> extractSuccessfulValue() {
    CRISP_TRY(auto value, Result<int>{42}, "unexpected failure");
    return value;
}

Result<int> extractConstValue() {
    CRISP_TRY(const auto value, Result<int>{42});
    return value;
}

Result<int> extractConstReference() {
    CRISP_TRY(const auto& value, Result<int>{42});
    return value;
}

Result<int> extractMutableReference() {
    CRISP_TRY(auto& value, Result<int>{41});
    ++value;
    return value;
}

Result<int> extractPointer() {
    int source = 42;
    CRISP_TRY(auto* value, Result<int*>{&source});
    return *value;
}

Result<int> extractConstPointer() {
    int source = 42;
    CRISP_TRY(const auto* value, Result<int*>{&source});
    return *value;
}

Result<int> extractIntoVariableExpression() {
    struct Holder {
        int value{0};
    } holder;

    CRISP_TRY(holder.value, Result<int>{42}, "unexpected failure");
    return holder.value;
}
} // namespace

TEST(ResultTest, Basic) {
    EXPECT_THAT(Result<int>(5), HasValue(5));
}

TEST(ResultTest, BasicError) {
    EXPECT_THAT(Result<int>{resultError("{}", "invalid path!")}, HasErrorWithMessageRegex("invalid path"));
}

TEST(ResultTest, ValueWithMoveOnly) {
    Result<MoveOnlyType> result(MoveOnlyType{});
    EXPECT_THAT(result, Not(HasError()));

    const auto value = std::move(result).unwrap();
    EXPECT_EQ(*value.x, 10);
}

TEST(ResultTest, TryPropagatesVoidResultError) {
    EXPECT_THAT(propagateFailure(), HasErrorWithMessageRegex("original failure"));
}

TEST(ResultTest, TryAddsContextToVoidResultError) {
    EXPECT_THAT(propagateFailureWithContext(), HasErrorWithMessageRegex("contextual failure: 17"));
}

TEST(ResultTest, TryContinuesAfterSuccessfulVoidResult) {
    EXPECT_THAT(continueAfterSuccess(), HasValue(42));
}

TEST(ResultTest, TryExtractsSuccessfulValue) {
    EXPECT_THAT(extractSuccessfulValue(), HasValue(42));
}

TEST(ResultTest, TrySupportsAutoDeclarationVariants) {
    EXPECT_THAT(extractConstValue(), HasValue(42));
    EXPECT_THAT(extractConstReference(), HasValue(42));
    EXPECT_THAT(extractMutableReference(), HasValue(42));
    EXPECT_THAT(extractPointer(), HasValue(42));
    EXPECT_THAT(extractConstPointer(), HasValue(42));
}

TEST(ResultTest, TryExtractsIntoVariableExpression) {
    EXPECT_THAT(extractIntoVariableExpression(), HasValue(42));
}
} // namespace crisp
