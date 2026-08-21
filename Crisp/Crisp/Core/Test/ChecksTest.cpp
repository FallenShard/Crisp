#include <Crisp/Core/Checks.hpp>

#include <cstdint>
#include <vector>

#include <gmock/gmock.h>

using ::testing::AllOf;
using ::testing::HasSubstr;

namespace {

// Death tests match against the child's stderr, but the default logger writes to stdout.
void routeLogsToStderr() {
    spdlog::drop_all();
    spdlog::set_default_logger(spdlog::stderr_color_st("checks-test"));
    spdlog::set_pattern("%v");
}

// clang-format off
// kLine must stay exactly two lines above the check it names, and the multi-line invocation must
// stay spread across several lines -- both are the subject of the tests below.
struct SingleLineFailure {
    static constexpr int kLine = __LINE__ + 2;
    static void run(const int a, const int b) {
        CRISP_CHECK_EQ(a, b);
    }
};

struct MultiLineFailure {
    static constexpr int kLine = __LINE__ + 2;
    static void run(const int a, const int b) {
        CRISP_CHECK_EQ(
            a,
            b,
            "operands spanning several source lines");
    }
};

// clang-format on

int g_messageArgumentEvaluations = 0;

int countedValue() {
    ++g_messageArgumentEvaluations;
    return 7;
}

constexpr int passThroughPositive(const int value) {
    CRISP_CHECK_GT(value, 0);
    return value;
}

// Checks are usable during constant evaluation: a violated invariant becomes a compile error,
// because the failure path is not constexpr.
static_assert(passThroughPositive(5) == 5);
static_assert(crisp::checkedCast<uint8_t>(255) == 255);

TEST(ChecksTest, ReportsTheLineOfTheCheckItself) {
    EXPECT_DEATH(
        {
            routeLogsToStderr();
            SingleLineFailure::run(1, 2);
        },
        HasSubstr(fmt::format("({}:", SingleLineFailure::kLine)));
}

TEST(ChecksTest, ReportsTheOpeningLineOfAMultiLineCheck) {
    EXPECT_DEATH(
        {
            routeLogsToStderr();
            MultiLineFailure::run(1, 2);
        },
        HasSubstr(fmt::format("({}:", MultiLineFailure::kLine)));
}

TEST(ChecksTest, ReportsTheOperandValues) {
    EXPECT_DEATH(
        {
            routeLogsToStderr();
            SingleLineFailure::run(1, 2);
        },
        AllOf(HasSubstr("lhs = 1"), HasSubstr("rhs = 2")));
}

TEST(ChecksTest, ReportsTheOperatorThatWasChecked) {
    EXPECT_DEATH(
        {
            routeLogsToStderr();
            const int value = 3;
            CRISP_CHECK_NE(value, 3);
        },
        HasSubstr("value != 3"));
}

TEST(ChecksTest, ReportsTheFormattedMessage) {
    EXPECT_DEATH(
        {
            routeLogsToStderr();
            CRISP_CHECK(false, "budget of {} exhausted", 42);
        },
        HasSubstr("budget of 42 exhausted"));
}

TEST(ChecksTest, DoesNotFormatTheMessageWhenTheCheckPasses) {
    g_messageArgumentEvaluations = 0;
    CRISP_CHECK(true, "unused {}", countedValue());
    EXPECT_EQ(g_messageArgumentEvaluations, 0);
}

TEST(ChecksTest, EvaluatesTheCheckedExpressionOnce) {
    int evaluations = 0;
    const auto next = [&evaluations] {
        ++evaluations;
        return 5;
    };

    CRISP_CHECK_GE_LT(next(), 0, 10);
    EXPECT_EQ(evaluations, 1);
}

TEST(ChecksTest, ParenthesizesTheOperands) {
    constexpr int kFlags = 0b010;

    // Without parentheses this parses as kFlags & (0b010 == 2), which is zero.
    CRISP_CHECK_EQ(kFlags & 0b010, 2);
    SUCCEED();
}

TEST(ChecksTest, ComparesMixedSignednessMathematically) {
    const int negative = -1;
    const uint32_t positive = 3;

    CRISP_CHECK_LT(negative, positive);
    SUCCEED();
}

TEST(ChecksTest, CatchesNegativeValuesAgainstUnsignedBounds) {
    EXPECT_DEATH(
        {
            routeLogsToStderr();
            const int negative = -1;
            CRISP_CHECK_GE(negative, 0u);
        },
        HasSubstr("Check failed"));
}

TEST(ChecksTest, SupportsTheShorthandChecks) {
    const std::vector<int> values{1, 2, 3};
    const std::vector<int> sameSize{4, 5, 6};
    const int* pointer = values.data();

    CRISP_CHECK_NOT_NULL(pointer);
    CRISP_CHECK_INDEX(2, values);
    CRISP_CHECK_SIZE_EQ(values, sameSize);
    CRISP_CHECK_NEAR(1.0F, 1.0001F, 0.001F);
    SUCCEED();
}

TEST(ChecksTest, ReportsTheIndexAndSizeOnAnOutOfBoundsIndex) {
    EXPECT_DEATH(
        {
            routeLogsToStderr();
            const std::vector<int> values(3, 0);
            CRISP_CHECK_INDEX(7, values);
        },
        AllOf(HasSubstr("index = 7"), HasSubstr("size = 3")));
}

TEST(ChecksTest, DevChecksCompileInEveryConfiguration) {
    const std::vector<int> values{1, 2, 3};

    CRISP_DEV_CHECK(!values.empty());
    CRISP_DEV_CHECK_EQ(values.size(), 3U);
    CRISP_DEV_CHECK_INDEX(1, values);
    CRISP_DEV_CHECK_GE_LT(values[0], 0, 10);
    SUCCEED();
}

TEST(ChecksTest, CheckedCastNarrowsValuesThatFit) {
    EXPECT_EQ(crisp::checkedCast<uint8_t>(255), 255);
    EXPECT_EQ(crisp::checkedCast<int8_t>(-128), -128);
}

TEST(ChecksTest, CheckedCastRejectsValuesThatDoNotFit) {
    EXPECT_DEATH(
        {
            routeLogsToStderr();
            (void)crisp::checkedCast<uint8_t>(256);
        },
        HasSubstr("value = 256"));
}

TEST(ChecksTest, CheckedCastRejectsNegativeValuesForUnsignedTargets) {
    EXPECT_DEATH(
        {
            routeLogsToStderr();
            (void)crisp::checkedCast<uint32_t>(-1);
        },
        HasSubstr("value = -1"));
}

} // namespace
