#include "LambertianBSDF.hpp"

#include "Samplers/Sampler.hpp"
#include <Crisp/Math/CoordinateFrame.hpp>
#include <Crisp/Math/Warp.hpp>

#include "Textures/TextureFactory.hpp"

namespace crisp
{
LambertianBSDF::LambertianBSDF(const VariantMap& params)
    : BSDF(Lobe::Diffuse)
{
    VariantMap texParams;
    Spectrum albedo(1.0f);
    if (params.contains("albedo"))
    {
        albedo = params.get<Spectrum>("albedo");
    }
    if (params.contains("reflectance"))
    {
        albedo = params.get<Spectrum>("reflectance");
    }
    texParams.insert("value", albedo);

    m_albedo = TextureFactory::create<Spectrum>("constant-spectrum", texParams);
}

void LambertianBSDF::setTexture(std::shared_ptr<Texture<Spectrum>> texture)
{
    if (texture->getName() == "albedo" || texture->getName() == "reflectance")
    {
        m_albedo = texture;
    }
}

Spectrum LambertianBSDF::eval(const BSDF::Sample& bsdfSample) const
{
    if (bsdfSample.measure != Measure::SolidAngle || CoordinateFrame::cosTheta(bsdfSample.wi) <= 0.0f ||
        CoordinateFrame::cosTheta(bsdfSample.wo) <= 0.0f)
        return 0.0f;

    return m_albedo->eval(bsdfSample.uv) * InvPI<> * CoordinateFrame::cosTheta(bsdfSample.wo);
}

Spectrum LambertianBSDF::sample(BSDF::Sample& bsdfSample, Sampler& sampler) const
{
    bsdfSample.wo = warp::squareToCosineHemisphere(sampler.next2D());
    bsdfSample.pdf = warp::squareToCosineHemispherePdf(bsdfSample.wo);
    bsdfSample.measure = Measure::SolidAngle;
    bsdfSample.sampledLobe = Lobe::Diffuse;
    bsdfSample.eta = 1.0f;

    // eval() / pdf() = albedo * invPI / (cosThetaO * invPI) * cosThetaO(subtension)
    // account for cosine subtension = just albedo
    return m_albedo->eval(bsdfSample.uv);
}

float LambertianBSDF::pdf(const BSDF::Sample& bsdfSample) const
{
    auto cosThetaO = CoordinateFrame::cosTheta(bsdfSample.wo);
    if (bsdfSample.measure != Measure::SolidAngle || cosThetaO <= 0.0f ||
        CoordinateFrame::cosTheta(bsdfSample.wi) <= 0.0f)
        return 0.0f;

    return InvPI<> * cosThetaO;
}
} // namespace crisp