#pragma once

#include <memory>
#include <string>
#include <functional>
#include <cmath>

#include "Texture.hpp"

namespace vesper
{
    template <typename T>
    class CheckerboardTexture : public Texture<T>
    {
    public:
        CheckerboardTexture(const VariantMap& variantMap = VariantMap());

        virtual T eval(const glm::vec2& uv) override
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