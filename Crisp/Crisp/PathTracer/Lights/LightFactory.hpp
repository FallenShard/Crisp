#pragma once

#include <memory>
#include <string>
#include <functional>

#include <Crisp/PathTracer/Core/VariantMap.hpp>

#include "Light.hpp"

namespace crisp
{
    class LightFactory
    {
    public:
        static std::unique_ptr<Light> create(std::string type, VariantMap parameters);
    };
}