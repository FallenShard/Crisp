#pragma once

#include <functional>
#include <memory>
#include <string>

#include <Crisp/PathTracer/Core/VariantMap.hpp>

#include <Crisp/PathTracer/PhaseFunctions/PhaseFunction.hpp>

namespace crisp {
class PhaseFunctionFactory {
public:
    static std::unique_ptr<PhaseFunction> create(std::string type, VariantMap parameters);
};
} // namespace crisp