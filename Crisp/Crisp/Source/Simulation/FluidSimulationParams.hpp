#pragma once

#include <glm/vec3.hpp>

namespace crisp
{
    struct FluidSimulationParams
    {
        float particleSpacing;
        float kernelSize;

        float restDensity;
        float stiffness;
        float mass;

        glm::vec3 gravity;

        float timeStep;
    };
}