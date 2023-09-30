#pragma once

#include <functional>
#include <memory>
#include <string>

#include <Crisp/PathTracer/Core/VariantMap.hpp>

#include "Light.hpp"

namespace crisp {
class LightFactory {
public:
    static std::unique_ptr<Light> create(std::string type, VariantMap parameters);
};
} // namespace crisp