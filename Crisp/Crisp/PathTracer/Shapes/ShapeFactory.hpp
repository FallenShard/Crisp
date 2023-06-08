#pragma once

#include <memory>
#include <string>
#include <functional>

#include <Crisp/PathTracer/Core/VariantMap.hpp>

#include "Shape.hpp"

namespace crisp
{
    class ShapeFactory
    {
    public:
        static std::unique_ptr<Shape> create(std::string type, VariantMap parameters);
    };
}