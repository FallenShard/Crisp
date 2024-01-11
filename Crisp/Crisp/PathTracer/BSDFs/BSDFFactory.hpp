#pragma once

#include <functional>
#include <memory>
#include <string>

#include <Crisp/PathTracer/BSDFs/BSDF.hpp>
#include <Crisp/PathTracer/Core/VariantMap.hpp>


namespace crisp {
class BSDFFactory {
public:
    static std::unique_ptr<BSDF> create(std::string type, VariantMap parameters);
};
} // namespace crisp