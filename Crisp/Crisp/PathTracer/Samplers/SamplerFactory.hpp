#pragma once

#include <memory>
#include <string>
#include <functional>

#include <Crisp/PathTracer/Core/VariantMap.hpp>

#include "Sampler.hpp"

namespace crisp
{
    class SamplerFactory
    {
    public:
        static std::unique_ptr<Sampler> create(std::string type, VariantMap parameters);
    };
}