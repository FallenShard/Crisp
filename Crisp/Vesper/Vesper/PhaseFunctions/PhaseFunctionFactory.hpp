#pragma once

#include <memory>
#include <string>
#include <functional>

#include "Core/VariantMap.hpp"

#include "PhaseFunction.hpp"

namespace crisp
{
    class PhaseFunctionFactory
    {
    public:
        static std::unique_ptr<PhaseFunction> create(std::string type, VariantMap parameters);
    };
}