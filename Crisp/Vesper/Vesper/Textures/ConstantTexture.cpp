#include "ConstantTexture.hpp"

namespace vesper
{
    template <>
    ConstantTexture<float>::ConstantTexture(const VariantMap& params)
        : Texture<float>(params)
    {
        m_value = params.get<float>("value", 1.0f);
    }

    template <>
    ConstantTexture<Spectrum>::ConstantTexture(const VariantMap& params)
        : Texture<Spectrum>(params)
    {
        m_value = params.get<Spectrum>("value", Spectrum(1.0f));
    }
}