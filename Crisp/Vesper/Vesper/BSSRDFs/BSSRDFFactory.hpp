#pragma once

#include <memory>
#include <string>
#include <functional>

#include "Core/VariantMap.hpp"

#include "BSSRDF.hpp"

namespace vesper
{
    class BSSRDFFactory
    {
    public:
        static std::unique_ptr<BSSRDF> create(std::string type, VariantMap parameters);
    };
}