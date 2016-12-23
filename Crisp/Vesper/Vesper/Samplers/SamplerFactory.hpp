#pragma once

#include <memory>
#include <string>
#include <functional>

#include "Core/VariantMap.hpp"

#include "Sampler.hpp"

namespace vesper
{
    class SamplerFactory
    {
    public:
        static std::unique_ptr<Sampler> create(std::string type, VariantMap parameters);
    };
}