#include "LambertianBSDF.hpp"

#include "Math/CoordinateFrame.hpp"
#include "Math/Warp.hpp"
#include "Samplers/Sampler.hpp"

#include "Textures/TextureFactory.hpp"

namespace vesper
{
    LambertianBSDF::LambertianBSDF(const VariantMap& params)
    {
        m_lobe = Lobe::Diffuse;

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
        if (bsdfSample.measure != Measure::SolidAngle ||
            CoordinateFrame::cosTheta(bsdfSample.wi) <= 0.0f ||
            CoordinateFrame::cosTheta(bsdfSample.wo) <= 0.0f)
            return 0.0f;

        return m_albedo->eval(bsdfSample.uv) * InvPI * CoordinateFrame::cosTheta(bsdfSample.wo);
    }

    Spectrum LambertianBSDF::sample(BSDF::Sample& bsdfSample, Sampler& sampler) const
    {
        bsdfSample.wo          = Warp::squareToCosineHemisphere(sampler.next2D());
        bsdfSample.pdf         = Warp::squareToCosineHemispherePdf(bsdfSample.wo);
        bsdfSample.measure     = Measure::SolidAngle;
        bsdfSample.sampledLobe = Lobe::Diffuse;
        bsdfSample.eta         = 1.0f;
        
        // eval() * cosThetaO / pdf() = albedo * invPI / (cosThetaI * invPI) * cosThetaI(subtension)
        // account for cosine subtension = just albedo
        return m_albedo->eval(bsdfSample.uv);
    }

    float LambertianBSDF::pdf(const BSDF::Sample& bsdfSample) const
    {
        auto cosThetaO = CoordinateFrame::cosTheta(bsdfSample.wo);
        auto cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
        if (bsdfSample.measure != Measure::SolidAngle ||
            cosThetaO <= 0.0f || cosThetaI <= 0.0f)
        {
            return 0.0f;
        }

        return InvPI * cosThetaO;
    }
}