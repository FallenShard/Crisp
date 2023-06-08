#pragma once

#include <memory>
#include <string>
#include <functional>

#include <Crisp/PathTracer/Core/VariantMap.hpp>

#include "BSDF.hpp"

namespace crisp
{
    class BSDFFactory
    {
    public:
        static std::unique_ptr<BSDF> create(std::string type, VariantMap parameters);
    };
}