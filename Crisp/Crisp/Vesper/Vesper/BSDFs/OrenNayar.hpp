//#pragma once
//
//#include "BSDF.hpp"
//#include "Textures/Texture.hpp"
//
//namespace crisp
//{
//    class OrenNayarBSDF : public BSDF
//    {
//    public:
//        OrenNayarBSDF(const VariantMap& params);
//        ~OrenNayarBSDF();
//
//        virtual void setTexture(std::shared_ptr<Texture<Spectrum>> texture) override;
//        virtual Spectrum eval(const BSDF::Sample& bsdfSample) const override;
//        virtual Spectrum sample(BSDF::Sample& bsdfSample, Sampler& sampler) const override;
//        virtual float pdf(const BSDF::Sample& bsdfSample) const override;
//        virtual unsigned int getType() const override;
//
//    private:
//        std::shared_ptr<Texture<Spectrum>> m_albedo;
//    };
//}
