#pragma once

#include <memory>
#include <string>
#include <functional>
#include <cmath>

#include "Texture.hpp"

namespace vesper
{
    namespace internal
    {
        template <typename T> struct Default1;
        template <typename T> struct Default2;

        template <> struct Default1<float> { static constexpr float value = 1.0f; };
        template <> struct Default2<float> { static constexpr float value = 0.2f; };

        template <> struct Default1<Spectrum> { static constexpr Spectrum value = Spectrum(1.0f, 0.5f, 0.5f); };
        template <> struct Default2<Spectrum> { static constexpr Spectrum value = Spectrum(0.5f, 0.5f, 1.0f); };
    }

    template <typename T>
    class CheckerboardTexture : public Texture<T>
    {
    public:
        CheckerboardTexture(const VariantMap& variantMap = VariantMap())
            : Texture<T>(variantMap)
        {
            m_offset = variantMap.get<glm::vec2>("offset", glm::vec2(0.0f));
            m_scale  = variantMap.get<glm::vec2>("scale", glm::vec2(1.0f));
            m_value1 = variantMap.get<T>("value1", internal::Default1<T>::value);
            m_value2 = variantMap.get<T>("value2", internal::Default2<T>::value);
        }

        virtual T eval(const glm::vec2& uv) const override
        {
            float u = std::fmodf(uv.x + m_offset.x * m_scale.x, 2.0f * m_scale.x);
            float v = std::fmodf(uv.y + m_offset.y * m_scale.y, 2.0f * m_scale.y);

            if (u < 0.0f) u += 2.0f * m_scale.x;
            if (v < 0.0f) v += 2.0f * m_scale.y;

            if ((u > m_scale.x && v > m_scale.y) || 
                (u < m_scale.x && v < m_scale.y))
                return m_value1;
            else
                return m_value2;
        }

    protected:
        glm::vec2 m_offset;
        glm::vec2 m_scale;
        T m_value1;
        T m_value2;
    };
}