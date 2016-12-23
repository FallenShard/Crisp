#pragma once

#include <memory>
#include <string>
#include <functional>

#include "Core/VariantMap.hpp"

#include "Light.hpp"

namespace vesper
{
    class LightFactory
    {
    public:
        static std::unique_ptr<Light> create(std::string type, VariantMap parameters);
    };
}