#pragma once

#include <gmock/gmock.h>

#include <Crisp/Core/Result.hpp>

namespace crisp::test
{
MATCHER(HasError, "Matches a Return<T, E> that contains an error type.")
{
    std::ignore = result_listener;
    return !arg.hasValue();
}
} // namespace crisp::test
