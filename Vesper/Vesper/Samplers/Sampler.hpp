#pragma once

#include <memory>

#include "Core/VariantMap.hpp"
#include <Crisp/Math/Headers.hpp>

namespace crisp
{
class Sampler
{
public:
    Sampler();
    virtual ~Sampler();

    virtual std::unique_ptr<Sampler> clone() const = 0;

    virtual void prepare() = 0;
    virtual void generate() = 0;
    virtual void advance() = 0;

    virtual float next1D() = 0;
    virtual glm::vec2 next2D() = 0;

    virtual size_t getSampleCount() const;

protected:
    size_t m_sampleCount;
};
} // namespace crisp