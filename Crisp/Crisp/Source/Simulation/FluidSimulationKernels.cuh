#pragma once

#include <vector>

#include <cuda_runtime.h>
#include <glm/glm.hpp>

#include "Simulation/PropertyBuffer.hpp"

namespace crisp
{
    struct SimulationParams
    {
        int numParticles;
        glm::ivec3 gridDims;

        float cellSize;
        float poly6Const;
        float spikyGradConst;
        float viscosityLaplaceConst;
    };

    class FluidSimulationKernels
    {
    public:
        static void computeGridLocations(PropertyBuffer<int>& gridIndices, PropertyBuffer<int>& particleIndices,
                                         const PropertyBuffer<glm::vec4>& positions, const SimulationParams& params);
        static void sortParticleIndicesByGridLocation(PropertyBuffer<int>& gridLocations, PropertyBuffer<int>& particleIndices);
        static void computeGridCellOffsets(PropertyBuffer<int>& gridOffsets, const PropertyBuffer<int>& gridLocations, const PropertyBuffer<int>& gridIndices);

        static void debugNeighborCount(PropertyBuffer<glm::vec4>& colors, const PropertyBuffer<glm::vec4>& pos, const PropertyBuffer<int>& gridOffsets, int3 gridDims, float cellSize);

        static void computeDensityAndPressure(PropertyBuffer<float>& densities,           PropertyBuffer<float>& pressures,
                                              const PropertyBuffer<glm::vec4>& positions, const PropertyBuffer<int>& gridOffsets,
                                              const PropertyBuffer<int>& gridLocations,   const PropertyBuffer<int>& particleIndices,
                                              const SimulationParams& params);
        
        static void computeForces(PropertyBuffer<glm::vec3>& forces, PropertyBuffer<glm::vec4>& colors,
                                  const PropertyBuffer<glm::vec4>& positions, const PropertyBuffer<int>& gridOffsets, const PropertyBuffer<int>& gridLocations, const PropertyBuffer<int>& particleIndices,
                                  const PropertyBuffer<float>& densities, const PropertyBuffer<float>& pressures, const PropertyBuffer<glm::vec3>& velocities,
                                  int3 gridDims, float cellSize, const glm::vec3& gravity, float viscosity);

        static void integrate(PropertyBuffer<glm::vec4>& pos, PropertyBuffer<glm::vec3>& vel,
                              const PropertyBuffer<glm::vec3>& forces, const PropertyBuffer<float>& densities, float3 fluidSpace, float timeStep);
    };
}
