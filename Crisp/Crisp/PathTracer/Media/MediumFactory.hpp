#pragma once

#include <functional>
#include <memory>
#include <string>

#include <Crisp/PathTracer/Core/VariantMap.hpp>

#include "Medium.hpp"

namespace crisp {
class MediumFactory {
public:
    static std::unique_ptr<Medium> create(std::string type, VariantMap parameters);
};
} // namespace crisp