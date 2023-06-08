#include "Shape.hpp"

#include <Crisp/PathTracer/BSSRDFs/BSSRDF.hpp>

namespace crisp
{
Shape::Shape()
    : m_light(nullptr)
    , m_bsdf(nullptr)
    , m_bssrdf(nullptr)
{
}

Shape::~Shape() {}

BoundingBox3 Shape::getBoundingBox() const
{
    return m_boundingBox;
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

void Shape::setBSSRDF(std::unique_ptr<BSSRDF> bssrdf)
{
    m_bssrdf = std::move(bssrdf);
}

const BSSRDF* Shape::getBSSRDF() const
{
    return m_bssrdf.get();
}

BSSRDF* Shape::getBSSRDF()
{
    return m_bssrdf.get();
}

void Shape::setMedium(Medium* medium)
{
    m_medium = medium;
}

const Medium* Shape::getMedium() const
{
    return m_medium;
}
} // namespace crisp
