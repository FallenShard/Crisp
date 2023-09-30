#pragma once

#include "BSDF.hpp"

namespace crisp {
class MirrorBSDF : public BSDF {
public:
    MirrorBSDF(const VariantMap& params = VariantMap());
    ~MirrorBSDF();

    virtual Spectrum eval(const BSDF::Sample& bsdfSample) const override;
    virtual Spectrum sample(BSDF::Sample& bsdfSample, Sampler& sampler) const override;
    virtual float pdf(const BSDF::Sample& bsdfSample) const override;
};
} // namespace crisp