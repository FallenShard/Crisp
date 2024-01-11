#pragma once

#include <functional>
#include <memory>
#include <string>

#include <Crisp/PathTracer/Core/VariantMap.hpp>
#include <Crisp/PathTracer/Textures/CheckerboardTexture.hpp>
#include <Crisp/PathTracer/Textures/ConstantTexture.hpp>
#include <Crisp/PathTracer/Textures/Texture.hpp>
#include <Crisp/PathTracer/Textures/UVTexture.hpp>

namespace crisp {
class TextureFactory {
public:
    template <typename T>
    static std::unique_ptr<Texture<T>> create(std::string type, VariantMap parameters) {
        if constexpr (std::is_same_v<T, float>) {
            if (type == "constant-float") {
                return std::make_unique<ConstantTexture<float>>(parameters);
            } else if (type == "checkerboard-float") {
                return std::make_unique<CheckerboardTexture<float>>(parameters);
            } else {
                std::cerr << "Unknown texture type \"" << type << "\" requested! Creating default constant texture"
                          << std::endl;
                return std::make_unique<ConstantTexture<float>>(parameters);
            }
        } else if constexpr (std::is_same_v<T, Spectrum>) {
            if (type == "constant-spectrum") {
                return std::make_unique<ConstantTexture<Spectrum>>(parameters);
            } else if (type == "checkerboard-spectrum") {
                return std::make_unique<CheckerboardTexture<Spectrum>>(parameters);
            } else if (type == "uv") {
                return std::make_unique<UVTexture>(parameters);
            } else {
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