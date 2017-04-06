//#include "SmoothConductor.hpp"
//
//#include "Math/CoordinateFrame.hpp"
//#include "Samplers/Sampler.hpp"
//#include "Core/Fresnel.hpp"
//
//namespace vesper
//{
//    SmoothConductorBSDF::SmoothConductorBSDF(const VariantMap& params)
//    {
//        m_intIOR = params.get("intIOR", Fresnel::getIOR(IndexOfRefraction::Glass));
//        m_extIOR = params.get("extIOR", Fresnel::getIOR(IndexOfRefraction::Air));
//    }
//
//    SmoothConductorBSDF::~SmoothConductorBSDF()
//    {
//    }
//
//    Spectrum SmoothConductorBSDF::eval(const BSDF::Sample& bsdfSample) const
//    {
//        return Spectrum(0.0f);
//    }
//
//    Spectrum SmoothConductorBSDF::sample(BSDF::Sample& bsdfSample, Sampler& sampler) const
//    {
//        // Get the fresnel coefficient
//        float cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
//        float reflCoeff = Fresnel::dielectric(cosThetaI, m_extIOR, m_intIOR);
//
//        // Set the measure to discrete
//        bsdfSample.measure = Measure::Discrete;
//        bsdfSample.sampledType = BSDF::Type::Delta;
//
//        float sample = sampler.next1D();
//
//        // If sample is less than fresnelCoeff, reflect
//        if (sample <= reflCoeff)
//        {
//            // Reflection in local coordinates
//            bsdfSample.wo = glm::vec3(-bsdfSample.wi.x, -bsdfSample.wi.y, bsdfSample.wi.z);
//            bsdfSample.eta = 1.0f;
//
//            return Spectrum(1.0f);
//        }
//
//        // otherwise, refract
//        // These may be swapped if we come "from the inside"
//        float etaExt = m_extIOR, etaInt = m_intIOR;
//
//        // Normal to use for refraction direction formula
//        glm::vec3 n(0.0f, 0.0f, 1.0f);
//        glm::vec3 wi = bsdfSample.wi;
//
//        // If the angle was negative, we're coming from the inside, update relevant variables
//        if (cosThetaI < 0.0f)
//        {
//            std::swap(etaExt, etaInt);
//            cosThetaI = -cosThetaI;
//            n = -n;
//        }
//
//        // Set eta ratio
//        float etaRatio = etaExt / etaInt;
//        bsdfSample.eta = etaRatio;
//
//        // Set outgoing direction
//        float sinThetaTSqr = etaRatio * etaRatio * (1.0f - cosThetaI * cosThetaI);
//        bsdfSample.wo = -etaRatio * (wi - cosThetaI * n) - n * sqrtf(1.0f - sinThetaTSqr);
//
//        // Return the bsdf sample
//        return Spectrum(etaRatio * etaRatio);
//    }
//
//    float SmoothConductorBSDF::pdf(const BSDF::Sample& bsdfSample) const
//    {
//        return 0.0f;
//    }
//
//    unsigned int SmoothConductorBSDF::getType() const
//    {
//        return BSDF::Type::Delta;
//    }
//}