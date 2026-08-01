#pragma once

#include <Crisp/PathTracer/Core/VariantMap.hpp>
#include <Crisp/PathTracer/Spectra/Spectrum.hpp>

namespace crisp {
template <typename T>
class Texture {
public:
    Texture(const VariantMap& /*params*/ = VariantMap()) {}

    virtual ~Texture() {}

    virtual T eval(const glm::vec2& uv) const = 0;
};
} // namespace crisp
