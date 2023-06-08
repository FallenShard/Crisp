#pragma once

#include <memory>
#include <string>
#include <functional>

#include <Crisp/PathTracer/Core/VariantMap.hpp>

#include "Integrator.hpp"

namespace crisp
{
    class IntegratorFactory
    {
    public:
        static std::unique_ptr<Integrator> create(std::string type, VariantMap parameters);
    };
}