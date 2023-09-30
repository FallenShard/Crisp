#pragma once

#include <functional>
#include <memory>
#include <string>

#include <Crisp/PathTracer/Core/VariantMap.hpp>

#include "Integrator.hpp"

namespace crisp {
class IntegratorFactory {
public:
    static std::unique_ptr<Integrator> create(std::string type, VariantMap parameters);
};
} // namespace crisp