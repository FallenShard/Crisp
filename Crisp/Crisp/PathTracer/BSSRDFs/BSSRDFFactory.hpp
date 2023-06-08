#pragma once

#include <memory>
#include <string>
#include <functional>

#include <Crisp/PathTracer/Core/VariantMap.hpp>

#include "BSSRDF.hpp"

namespace crisp
{
    class BSSRDFFactory
    {
    public:
        static std::unique_ptr<BSSRDF> create(std::string type, VariantMap parameters);
    };
}