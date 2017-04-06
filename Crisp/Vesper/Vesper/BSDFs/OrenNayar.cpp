//#include "OrenNayar.hpp"
//
//#include "Math/CoordinateFrame.hpp"
//#include "Math/Warp.hpp"
//#include "Samplers/Sampler.hpp"
//
//#include "Textures/TextureFactory.hpp"
//
//namespace vesper
//{
//    OrenNayarBSDF::OrenNayarBSDF(const VariantMap& params)
//    {
//        VariantMap texParams;
//        Spectrum albedo(1.0f);
//        if (params.contains("albedo"))
//        {
//            albedo = params.get<Spectrum>("albedo");
//        }
//        if (params.contains("reflectance"))
//        {
//            albedo = params.get<Spectrum>("reflectance");
//        }
//        texParams.insert("value", albedo);
//
//        m_albedo = TextureFactory<Spectrum>::create("constant", texParams);
//    }
//
//    OrenNayarBSDF::~OrenNayarBSDF()
//    {
//    }
//
//    void OrenNayarBSDF::setTexture(std::shared_ptr<Texture<Spectrum>> texture)
//    {
//        if (texture->getName() == "albedo" || texture->getName() == "reflectance")
//        {
//            m_albedo = texture;
//        }
//    }
//
//    Spectrum OrenNayarBSDF::eval(const BSDF::Sample& bsdfSample) const
//    {
//        if (bsdfSample.measure != Measure::SolidAngle ||
//            CoordinateFrame::cosTheta(bsdfSample.wi) <= 0.0f ||
//            CoordinateFrame::cosTheta(bsdfSample.wo) <= 0.0f)
//            return 0.0f;
//
//        return m_albedo->eval(bsdfSample.uv) * InvPI;
//    }
//
//    Spectrum OrenNayarBSDF::sample(BSDF::Sample& bsdfSample, Sampler& sampler) const
//    {
//        bsdfSample.measure = Measure::SolidAngle;
//        bsdfSample.sampledType = Type::Diffuse;
//        bsdfSample.eta = 1.0f;
//        bsdfSample.wo = Warp::squareToCosineHemisphere(sampler.next2D());
//
//        // eval() * cosThetaO / pdf() = albedo * invPI / (cosThetaI * invPI) * cosThetaI(subtension)
//        // account for cosine subtension = just albedo
//        return m_albedo->eval(bsdfSample.uv);
//    }
//
//    float OrenNayarBSDF::pdf(const BSDF::Sample& bsdfSample) const
//    {
//        auto cosThetaO = CoordinateFrame::cosTheta(bsdfSample.wo);
//        auto cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
//        if (bsdfSample.measure != Measure::SolidAngle ||
//            cosThetaO <= 0.0f || cosThetaI <= 0.0f)
//        {
//            return 0.0f;
//        }
//
//        return InvPI * cosThetaO;
//    }
//
//    unsigned int OrenNayarBSDF::getType() const
//    {
//        return Type::Diffuse;
//    }
//}