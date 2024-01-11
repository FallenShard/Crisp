#include <Crisp/PathTracer/Media/Medium.hpp>

namespace crisp {
Medium::Medium() {}

Medium::~Medium() {}

void Medium::setShape(Shape* shape) {
    m_shape = shape;
}

void Medium::setPhaseFunction(PhaseFunction* phaseFunction) {
    m_phaseFunction = phaseFunction;
}

PhaseFunction* Medium::getPhaseFunction() const {
    return m_phaseFunction;
}
} // namespace crisp