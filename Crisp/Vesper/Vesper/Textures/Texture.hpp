#pragma once

#include <memory>
#include <string>
#include <functional>

#include "Core/VariantMap.hpp"
#include "Spectrums/Spectrum.hpp"

namespace vesper
{
    template <typename T>
    class Texture
    {
    public:
        Texture(const VariantMap& params = VariantMap())
        {
            m_name = params.get<std::string>("name", "");
        }

        virtual ~Texture() {}

        virtual T eval(const glm::vec2& uv) const = 0;

        std::string getName() const
        {
            return m_name;
        }

    protected:
        std::string m_name;

    };
}