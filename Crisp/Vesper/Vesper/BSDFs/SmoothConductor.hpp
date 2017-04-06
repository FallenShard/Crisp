//#pragma once
//
//#include "BSDF.hpp"
//
//namespace vesper
//{
//    class SmoothConductorBSDF : public BSDF
//    {
//    public:
//        SmoothConductorBSDF(const VariantMap& params = VariantMap());
//        ~SmoothConductorBSDF();
//
//        virtual Spectrum eval(const BSDF::Sample& bsdfSample) const override;
//        virtual Spectrum sample(BSDF::Sample& bsdfSample, Sampler& sampler) const override;
//        virtual float pdf(const BSDF::Sample& bsdfSample) const override;
//        virtual unsigned int getType() const override;
//
//    private:
//        float m_intIOR;
//        float m_extIOR;
//    };
//}