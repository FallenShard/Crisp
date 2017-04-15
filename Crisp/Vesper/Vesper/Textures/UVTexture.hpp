#pragma once

#include <memory>
#include <string>
#include <functional>
#include <cmath>

#include "Texture.hpp"

namespace vesper
{
    class UVTexture : public Texture<Spectrum>
    {
    public:
        UVTexture(const VariantMap& variantMap = VariantMap());

        virtual Spectrum eval(const glm::vec2& uv) const override;
    };
}