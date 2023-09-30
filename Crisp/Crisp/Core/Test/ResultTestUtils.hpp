#pragma once

#include <gmock/gmock.h>

#include <Crisp/Core/Result.hpp>

namespace crisp::test {
MATCHER(HasError, "Matches a Return<T, E> that contains an error type.") {
    std::ignore = result_listener;
    return !arg.hasValue();
}

MATCHER_P(HasErrorWithMessageRegex, msgRegex, "") {
    if (arg.hasValue()) {
        return false;
    }

    return ::testing::ExplainMatchResult(testing::ContainsRegex(msgRegex), arg.getError(), result_listener);
}

MATCHER_P(HasValue, value, "") {
    if (!arg.hasValue()) {
        return false;
    }

    return ::testing::ExplainMatchResult(testing::Eq(value), *arg, result_listener);
}

} // namespace crisp::test
