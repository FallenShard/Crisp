#include "Medium.hpp"

namespace vesper
{
    Medium::Medium()
    {
    }

    Medium::~Medium()
    {
    }

    void Medium::setShape(Shape* shape)
    {
        m_shape = shape;
    }

    void Medium::setPhaseFunction(PhaseFunction* phaseFunction)
    {
        m_phaseFunction = phaseFunction;
    }

    PhaseFunction* Medium::getPhaseFunction() const
    {
        return m_phaseFunction;
    }
}