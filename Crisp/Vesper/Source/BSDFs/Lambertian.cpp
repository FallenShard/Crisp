#include "Lambertian.hpp"

#include "Math/CoordinateFrame.hpp"
#include "Math/Warp.hpp"
#include "Samplers/Sampler.hpp"

namespace vesper
{
    LambertianBSDF::LambertianBSDF(const VariantMap& params)
    {
        m_reflectance = Spectrum(params.get("reflectance", glm::vec3(1.0f)));
    }

    LambertianBSDF::~LambertianBSDF()
    {
    }

    Spectrum LambertianBSDF::eval(const BSDF::Sample& bsdfSample) const
    {
        if (bsdfSample.measure != Measure::SolidAngle ||
            CoordinateFrame::cosTheta(bsdfSample.wi) <= 0.0f ||
            CoordinateFrame::cosTheta(bsdfSample.wo) <= 0.0f)
            return 0.0f;

        return m_reflectance * InvPI;
    }

    Spectrum LambertianBSDF::sample(BSDF::Sample& bsdfSample, Sampler& sampler) const
    {
        bsdfSample.measure = Measure::SolidAngle;
        bsdfSample.sampledType = Type::Diffuse;
        bsdfSample.eta = 1.0f;
        bsdfSample.wo = Warp::squareToCosineHemisphere(sampler.next2D());
        // eval() * cosThetaO / pdf() = albedo * invPI / (cosThetaI * invPI) * cosThetaI(subtension)
        // account for cosine subtension = just albedo :)
        return m_reflectance;
    }

    float LambertianBSDF::pdf(const BSDF::Sample& bsdfSample) const
    {
        if (bsdfSample.measure != Measure::SolidAngle ||
            CoordinateFrame::cosTheta(bsdfSample.wi) <= 0.0f ||
            CoordinateFrame::cosTheta(bsdfSample.wo) <= 0.0f)
            return 0.0f;

        return InvPI * CoordinateFrame::cosTheta(bsdfSample.wo);
    }

    unsigned int LambertianBSDF::getType() const
    {
        return Type::Diffuse;
    }
}