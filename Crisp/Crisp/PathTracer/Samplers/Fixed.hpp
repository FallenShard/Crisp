#pragma once

#include <array>
#include <random>

#include "Sampler.hpp"

namespace crisp {
class FixedSampler : public Sampler {
public:
    FixedSampler(const VariantMap& attribs = VariantMap());
    ~FixedSampler();

    virtual std::unique_ptr<Sampler> clone() const override;

    virtual void prepare() override;
    virtual void generate() override;
    virtual void advance() override;

    virtual float next1D() override;
    virtual glm::vec2 next2D() override;

private:
    inline float nextFloat();

    std::array<float, 3> m_values;
    size_t m_index;
};
} // namespace crisp