#include "BSDF.hpp"

namespace crisp
{
    BSDF::BSDF(LobeFlags lobeFlags)
        : m_lobe(lobeFlags)
    {
    }

    BSDF::~BSDF()
    {
    }

    void BSDF::setTexture(std::shared_ptr<Texture<float>> texture)
    {
    }

    void BSDF::setTexture(std::shared_ptr<Texture<Spectrum>> texture)
    {
    }

    LobeFlags BSDF::getLobeType() const
    {
        return m_lobe;
    }
}