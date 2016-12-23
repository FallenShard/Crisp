#include "Shape.hpp"

namespace vesper
{
    Shape::Shape()
        : m_light(nullptr)
        , m_bsdf(nullptr)
    {
    }

    void Shape::setLight(Light* light)
    {
        m_light = light;
    }

    const Light* Shape::getLight() const
    {
        return m_light;
    }

    void Shape::setBSDF(BSDF* bsdf)
    {
        m_bsdf = bsdf;
    }

    const BSDF* Shape::getBSDF() const
    {
        return m_bsdf;
    }
}
