#pragma once

#include <memory>
#include <string>
#include <functional>

#include "Core/VariantMap.hpp"

#include "Texture.hpp"
#include "ConstantTexture.hpp"
#include "CheckerboardTexture.hpp"
#include "UVTexture.hpp"

namespace vesper
{
    class TextureFactory
    {
    public:
        template <typename T>
        static std::unique_ptr<Texture<T>> create(std::string type, VariantMap parameters);

        template <>
        static std::unique_ptr<Texture<float>> create(std::string type, VariantMap parameters)
        {
            if (type == "constant-float")
            {
                return std::make_unique<ConstantTexture<float>>(parameters);
            }
            else if (type == "checkerboard-float")
            {
                return std::make_unique<CheckerboardTexture<float>>(parameters);
            }
            else
            {
                std::cerr << "Unknown texture type \"" << type << "\" requested! Creating default constant texture" << std::endl;
                return std::make_unique<ConstantTexture<float>>(parameters);
            }
        }

        template <>
        static std::unique_ptr<Texture<Spectrum>> create(std::string type, VariantMap parameters)
        {
            if (type == "constant-spectrum")
            {
                return std::make_unique<ConstantTexture<Spectrum>>(parameters);
            }
            else if (type == "checkerboard-spectrum")
            {
                return std::make_unique<CheckerboardTexture<Spectrum>>(parameters);
            }
            else if (type == "uv")
            {
                return std::make_unique<UVTexture>(parameters);
            }
            else
            {
                std::cerr << "Unknown texture type \"" << type << "\" requested! Creating default constant texture" << std::endl;
                return std::make_unique<ConstantTexture<Spectrum>>(parameters);
            }
        }
    };
}