#pragma once

#include <Crisp/PathTracer/Textures/Texture.hpp>

#include <cmath>
#include <functional>
#include <memory>
#include <string>

namespace crisp {
class UVTexture : public Texture<Spectrum> {
public:
    UVTexture(const VariantMap& variantMap = VariantMap());

    Spectrum eval(const glm::vec2& uv) const override;
};
} // namespace crisp