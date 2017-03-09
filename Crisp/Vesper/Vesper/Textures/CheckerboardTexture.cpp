#include "CheckerboardTexture.hpp"

namespace vesper
{
    template <>
    CheckerboardTexture<float>::CheckerboardTexture(const VariantMap& params)
        : Texture<float>(params)
    {
        m_offset = params.get<glm::vec2>("offset", glm::vec2(0.0f));
        m_scale  = params.get<glm::vec2>("scale", glm::vec2(1.0f));
        m_value1 = params.get<float>("value1", 1.0f);
        m_value2 = params.get<float>("value2", 0.2f);
    }

    template <>
    CheckerboardTexture<Spectrum>::CheckerboardTexture(const VariantMap& params)
        : Texture<Spectrum>(params)
    {
        m_offset = params.get<glm::vec2>("offset", glm::vec2(0.0f));
        m_scale  = params.get<glm::vec2>("scale", glm::vec2(1.0f));
        m_value1 = params.get<Spectrum>("value1", Spectrum(1.0f, 0.5f, 0.5f));
        m_value2 = params.get<Spectrum>("value2", Spectrum(0.5f, 0.5f, 1.0f));
    }
}