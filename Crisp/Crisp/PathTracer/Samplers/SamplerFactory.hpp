#pragma once

#include <functional>
#include <memory>
#include <string>

#include <Crisp/PathTracer/Core/VariantMap.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>

namespace crisp
{
class SamplerFactory
{
public:
    static std::unique_ptr<Sampler> create(std::string type, VariantMap parameters);
};
} // namespace crisp