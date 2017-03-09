#pragma once

#include <memory>
#include <string>
#include <functional>

#include "Core/VariantMap.hpp"

#include "Texture.hpp"
#include "ConstantTexture.hpp"
#include "CheckerboardTexture.hpp"

namespace vesper
{
    template <typename T>
    class TextureFactory
    {
    public:
        static std::unique_ptr<Texture<T>> create(std::string type, VariantMap parameters)
        {
            if (type == "constant")
            {
                return std::make_unique<ConstantTexture<T>>(parameters);
            }
            else if (type == "checkerboard")
            {
                return std::make_unique<CheckerboardTexture<T>>(parameters);
            }
            {
                std::cerr << "Unknown texture type \"" << type << "\" requested! Creating default constant texture" << std::endl;
                return std::make_unique<ConstantTexture<T>>(parameters);
            }
        }
    };
}