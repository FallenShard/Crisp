#pragma once

#include <functional>
#include <memory>
#include <string>

#include "Core/VariantMap.hpp"

#include "CheckerboardTexture.hpp"
#include "ConstantTexture.hpp"
#include "Texture.hpp"
#include "UVTexture.hpp"

namespace crisp
{
class TextureFactory
{
public:
    template <typename T>
    static std::unique_ptr<Texture<T>> create(std::string type, VariantMap parameters)
    {
        if constexpr (std::is_same_v<T, float>)
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
                std::cerr << "Unknown texture type \"" << type << "\" requested! Creating default constant texture"
                          << std::endl;
                return std::make_unique<ConstantTexture<float>>(parameters);
            }
        }
        else if constexpr (std::is_same_v<T, Spectrum>)
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
                std::cerr << "Unknown texture type \"" << type << "\" requested! Creating default constant texture"
                          << std::endl;
                return std::make_unique<ConstantTexture<Spectrum>>(parameters);
            }
        }

        /*std::cerr << "Unknown texture type" << std::endl;
        if (std::sin(1) < 1)
            std::terminate();*/
        // return nullptr;
    }
};
} // namespace crisp