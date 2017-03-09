#pragma once

#include "Texture.hpp"

namespace vesper
{
    template <typename T>
    class ConstantTexture : public Texture<T>
    {
    public:
        ConstantTexture(const VariantMap& variantMap = VariantMap());

        virtual T eval(const glm::vec2& uv) override
        {
            return m_value;
        }

    protected:
        T m_value;
    };
}