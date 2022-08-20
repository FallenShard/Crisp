#pragma once

#include "BSSRDF.hpp"
#include "Core/VariantMap.hpp"

#include "IrradianceTree.hpp"
#include <Crisp/Math/Octree.hpp>

namespace crisp
{
class DipoleBSSRDF : public BSSRDF
{
public:
    DipoleBSSRDF(const VariantMap& params);
    virtual void preprocess(const Shape* shape, const Scene* scene) override;
    virtual Spectrum eval(const Sample& sample) const override;

private:
    Spectrum m_sigmaA;
    Spectrum m_sigmaPrimeS;
    Spectrum m_sigmaPrimeT;
    float m_eta;

    Spectrum m_sigmaTr;
    Spectrum m_alphaPrime;

    Spectrum m_zPos;
    Spectrum m_zNeg;

    float m_minDist;
    float m_maxError;
    float m_distMultiplier;
    int m_sampleTrials;

    bool m_debugMode;

    float m_fdr;

    std::unique_ptr<Octree<SurfacePoint>> m_octree;
    std::unique_ptr<IrradianceTree> m_ssOctree;
    std::vector<IrradiancePoint> m_irrPts;
};
} // namespace crisp