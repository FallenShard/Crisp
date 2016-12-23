#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "Core/VariantMap.hpp"

namespace vesper
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
}