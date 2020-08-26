#pragma once

#include "Texture.hpp"

namespace crisp
{
    template <typename T>
    class ConstantTexture : public Texture<T>
    {
    public:
        ConstantTexture(const VariantMap& variantMap = VariantMap())
            : Texture<T>(variantMap)
        {
            m_value = variantMap.get<T>("value", T(1.0f));
        }

        virtual T eval(const glm::vec2& /*uv*/) const override
        {
            return m_value;
        }

    protected:
        T m_value;
    };
}