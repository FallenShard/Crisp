#pragma once

#include <memory>
#include <string>
#include <functional>

#include "Core/VariantMap.hpp"

#include "Medium.hpp"

namespace vesper
{
    class MediumFactory
    {
    public:
        static std::unique_ptr<Medium> create(std::string type, VariantMap parameters);
    };
}