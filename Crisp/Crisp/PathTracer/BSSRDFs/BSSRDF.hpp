#pragma once

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Spectra/Spectrum.hpp>

namespace crisp {
class Shape;

namespace pt {
class Scene;
}

class BSSRDF {
public:
    struct Sample {
        glm::vec3 p;
        glm::vec3 n;
        glm::vec3 wo;

        Sample(const glm::vec3& p, const glm::vec3& n, const glm::vec3& wo)
            : p(p)
            , n(n)
            , wo(wo) {}
    };

    virtual ~BSSRDF() {}

    virtual void preprocess(const Shape* shape, const pt::Scene* scene) = 0;
    virtual Spectrum eval(const Sample& sample) const = 0;
};
} // namespace crisp