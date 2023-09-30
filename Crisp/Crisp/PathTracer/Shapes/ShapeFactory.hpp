#pragma once

#include <functional>
#include <memory>
#include <string>

#include <Crisp/PathTracer/Core/VariantMap.hpp>

#include "Shape.hpp"

namespace crisp {
class ShapeFactory {
public:
    static std::unique_ptr<Shape> create(std::string type, VariantMap parameters);
};
} // namespace crisp