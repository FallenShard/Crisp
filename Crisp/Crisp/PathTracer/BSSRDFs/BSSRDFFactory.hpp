#pragma once

#include <functional>
#include <memory>
#include <string>

#include <Crisp/PathTracer/Core/VariantMap.hpp>

#include <Crisp/PathTracer/BSSRDFs/BSSRDF.hpp>

namespace crisp {
class BSSRDFFactory {
public:
    static std::unique_ptr<BSSRDF> create(std::string type, VariantMap parameters);
};
} // namespace crisp