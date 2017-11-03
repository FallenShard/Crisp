#include "FluidSimulationKernels.cuh"

#include <device_launch_parameters.h>

#include <thrust/sort.h>
#include <thrust/binary_search.h>
#include <thrust/device_ptr.h>

#include "Models/FluidSimulationParams.hpp"

namespace crisp
{
    namespace
    {
        __device__ __forceinline__ float poly6(float x)
        {
            if (x >= h)
                return 0.0f;

            float val = h2 - x * x;
            return poly6Const * val * val * val;
        }

        __device__ __forceinline__ glm::vec3 spikyGrad(glm::vec3 vec, float x)
        {
            return spikyGradConst * (h - x) * (h - x) * (1.0f / x) * vec;
        }

        __device__ __forceinline__ float viscoLaplacian(float x)
        {
            return viscosityLaplaceConst * (h - x);
        }

        __device__ __forceinline__ int3 calculateGridPosition(glm::vec4 particlePosition, float cellSize)
        {
            int3 gridPosition;
            gridPosition.x = static_cast<int>(particlePosition.x / cellSize);
            gridPosition.y = static_cast<int>(particlePosition.y / cellSize);
            gridPosition.z = static_cast<int>(particlePosition.z / cellSize);
            return gridPosition;
        }

        __device__ __forceinline__ int getGridLinearIndex(int3 gridPosition, const glm::ivec3& gridDims)
        {
            return gridPosition.z * gridDims.y * gridDims.x + gridPosition.y * gridDims.x + gridPosition.x;
        }

        __device__ __forceinline__ int getGridLinearIndex(int x, int y, int z, const glm::ivec3& gridDims)
        {
            return z * gridDims.y * gridDims.x + y * gridDims.x + x;
        }

        __device__ __forceinline__ int getGridLinearIndex(int x, int y, int z, const int3& gridDims)
        {
            return z * gridDims.y * gridDims.x + y * gridDims.x + x;
        }

        __device__ __forceinline__ glm::vec3 getHeatMapColor(float value) {
            if (value > 250.0f) {
                return glm::vec3(1.0f, 0.0f, 0.0f);
            }
            else if (value > 150) {
                return glm::mix(glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), (value - 150) / 100.0f);
            }
            else if (value > 100) {
                return glm::mix(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), (value - 100) / 50.0f);
            }
            else if (value > 50) {
                return glm::mix(glm::vec3(0.0f, 1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), (value - 50) / 50.0f);
            }
            else
                return glm::mix(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 1.0f), (value) / 50.0f);
        }
    }

    __global__ void computeGridIndex(int* gridIndices, int* particleIndices, const glm::vec4* positions, SimulationParams params)
    {
        int particleIdx = blockIdx.x * blockDim.x + threadIdx.x;
        if (particleIdx >= params.numParticles)
            return;
    
        glm::vec4 position = positions[particleIdx];
        int3 gridPosition = calculateGridPosition(position, params.cellSize);
        int linearGridIdx = getGridLinearIndex(gridPosition, params.gridDims);

        gridIndices[particleIdx]     = linearGridIdx;
        particleIndices[particleIdx] = particleIdx;
    }
    
    void FluidSimulationKernels::computeGridLocations(PropertyBuffer<int>& gridIndices, PropertyBuffer<int>& particleIndices, const PropertyBuffer<glm::vec4>& positions, const SimulationParams& params)
    {
        int numParticles = static_cast<int>(positions.getSize());
        int blockSize = 256;
        int gridSize = (numParticles - 1) / blockSize + 1;
        computeGridIndex<<<gridSize, blockSize>>>(gridIndices.getDeviceBuffer(), particleIndices.getDeviceBuffer(),
            positions.getDeviceBuffer(), params);
    }

    void FluidSimulationKernels::sortParticleIndicesByGridLocation(PropertyBuffer<int>& gridLocations, PropertyBuffer<int>& particleIndices)
    {
        auto gridIdxPtr     = thrust::device_pointer_cast(gridLocations.getDeviceBuffer());
        auto particleIdxPtr = thrust::device_pointer_cast(particleIndices.getDeviceBuffer());
        int numParticles    = static_cast<int>(particleIndices.getSize());
        thrust::sort_by_key(gridIdxPtr, gridIdxPtr + numParticles, particleIdxPtr);
    }

    void FluidSimulationKernels::computeGridCellOffsets(PropertyBuffer<int>& gridOffsets, const PropertyBuffer<int>& gridLocations, const PropertyBuffer<int>& gridIndices)
    {
        auto gridLocPtr        = thrust::device_pointer_cast(gridLocations.getDeviceBuffer());
        auto gridIdxPtr        = thrust::device_pointer_cast(gridIndices.getDeviceBuffer());
        auto gridCellOffsetPtr = thrust::device_pointer_cast(gridOffsets.getDeviceBuffer());
        int numCells = static_cast<int>(gridIndices.getSize());

        thrust::lower_bound(gridLocPtr, gridLocPtr + numCells,
                            gridIdxPtr, gridIdxPtr + numCells,
                            gridCellOffsetPtr);
    }

    __global__ void debugNbrs(glm::vec4* colors, const glm::vec4* positions, const int* gridOffsets, int numParticles, int3 gridDims, float cellSize)
    {
        int idx = blockIdx.x * blockDim.x + threadIdx.x;
        if (idx >= numParticles)
            return;

        glm::vec4 position = positions[idx];
        int3 gridPosition = calculateGridPosition(position, cellSize);

        int3 lower;
        lower.x = max(0, gridPosition.x - 1);
        lower.y = max(0, gridPosition.y - 1);
        lower.z = max(0, gridPosition.z - 1);
        int3 upper;
        upper.x = min(gridDims.x - 1, gridPosition.x + 1);
        upper.y = min(gridDims.y - 1, gridPosition.y + 1);
        upper.z = min(gridDims.z - 1, gridPosition.z + 1);

        int numCells = gridDims.x * gridDims.y * gridDims.z;
        int count = 0;
        for (int z = lower.z; z <= upper.z; ++z)
        {
            for (int y = lower.y; y <= upper.y; ++y)
            {
                for (int x = lower.x; x <= upper.x; ++x)
                {
                    int linearCellIdx = getGridLinearIndex(x, y, z, gridDims);
                    int cellOffset = gridOffsets[linearCellIdx];
                    int nextCellOffset = linearCellIdx == numCells - 1 ? numParticles : gridOffsets[linearCellIdx + 1];
                    for (int i = cellOffset; i < nextCellOffset; ++i)
                    {
                        count++;
                    }
                }
            }
        }

        if (count == 64)
            colors[idx] = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        else if (count == 3 * 3 * 2 * 8)
            colors[idx] = glm::vec4(0.0f, 1.0f, 1.0f, 0.0f);
        else
            colors[idx] = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    }

    void FluidSimulationKernels::debugNeighborCount(PropertyBuffer<glm::vec4>& colors, const PropertyBuffer<glm::vec4>& pos, const PropertyBuffer<int>& gridOffsets, int3 gridDims, float cellSize)
    {
        int n = static_cast<int>(colors.getSize());
        int blockSize = 256;
        int gridSize = (n - 1) / blockSize + 1;
        debugNbrs<<<gridSize, blockSize>>>(colors.getDeviceBuffer(), pos.getDeviceBuffer(), gridOffsets.getDeviceBuffer(), n, gridDims, cellSize);
    }

    __global__ void computeDensityAndPressureKernel(float* densities, float* pressures, glm::vec4* normals,
        const glm::vec4* positions, const int* gridOffsets, const int* gridLocations, const int* particleIndices,
        SimulationParams params)
    {
        int numParticles = params.numParticles;
        int idx = blockIdx.x * blockDim.x + threadIdx.x;
        if (idx >= numParticles)
            return;

        glm::vec4 position = positions[idx];
        int3 gridPosition = calculateGridPosition(position, params.cellSize);
        glm::ivec3 gridDims = params.gridDims;
        float poly6Const = params.poly6Const;
        

        int3 lower;
        lower.x = max(0, gridPosition.x - 1);
        lower.y = max(0, gridPosition.y - 1);
        lower.z = max(0, gridPosition.z - 1);
        int3 upper;
        upper.x = min(gridDims.x - 1, gridPosition.x + 1);
        upper.y = min(gridDims.y - 1, gridPosition.y + 1);
        upper.z = min(gridDims.z - 1, gridPosition.z + 1);

        int numCells = gridDims.x * gridDims.y * gridDims.z;
        glm::vec3 posI = position;
        float density = 0.0f;
        
        glm::vec3 ni;
        for (int z = lower.z; z <= upper.z; ++z)
        {
            for (int y = lower.y; y <= upper.y; ++y)
            {
                for (int x = lower.x; x <= upper.x; ++x)
                {
                    int linearCellIdx = getGridLinearIndex(x, y, z, gridDims);
                    int cellOffset = gridOffsets[linearCellIdx];
                    int nextCellOffset = linearCellIdx == numCells - 1 ? numParticles : gridOffsets[linearCellIdx + 1];
                    for (int i = cellOffset; i < nextCellOffset; ++i)
                    {
                        int j = particleIndices[i];
                        glm::vec3 posJ = positions[j];

                        glm::vec3 diff = posI - posJ;
                        float dist = glm::length(diff);

                        density += mass * poly6(dist);

                        //if (dist > 0.0f && dist < h)
                        //    ni += mass / densities[idx] * spikyGrad(diff, dist);
                    }
                }
            }
        }

        densities[idx] = density;
        pressures[idx] = max(0.0f, stiffness * (density - restDensity));
        normals[idx] = glm::vec4(h * ni, 1.0f);
        //pressures[idx] = stiffness * (density - restDensity);
    }

    void FluidSimulationKernels::computeDensityAndPressure(PropertyBuffer<float>& densities,           PropertyBuffer<float>& pressures, PropertyBuffer<glm::vec4>& normals,
                                            const PropertyBuffer<glm::vec4>& positions, const PropertyBuffer<int>& gridOffsets,
                                            const PropertyBuffer<int>& gridLocations,   const PropertyBuffer<int>& particleIndices,
                                            const SimulationParams& params)
    {
        int numParticles = static_cast<int>(positions.getSize());
        int blockSize = 256;
        int gridSize = (numParticles - 1) / blockSize + 1;
        computeDensityAndPressureKernel<<<gridSize, blockSize>>>(densities.getDeviceBuffer(), pressures.getDeviceBuffer(), normals.getDeviceBuffer(),
            positions.getDeviceBuffer(), gridOffsets.getDeviceBuffer(), gridLocations.getDeviceBuffer(), particleIndices.getDeviceBuffer(),
            params);
    }

    __global__ void computeForcesKernel(glm::vec3* forces, glm::vec4* colors,
        const glm::vec4* positions, const int* gridOffsets, const int* gridLocations, const int* particleIndices,
        const float* densities, const float* pressures, const glm::vec3* velocities, const glm::vec4* normals,
        int numParticles, int3 gridDims, float cellSize, const glm::vec3 gravity, float viscosity)
    {
        int idx = blockIdx.x * blockDim.x + threadIdx.x;
        if (idx >= numParticles)
            return;

        glm::vec4 position = positions[idx];
        int3 gridPosition = calculateGridPosition(position, cellSize);


        int3 lower;
        lower.x = max(0, gridPosition.x - 1);
        lower.y = max(0, gridPosition.y - 1);
        lower.z = max(0, gridPosition.z - 1);
        int3 upper;
        upper.x = min(gridDims.x - 1, gridPosition.x + 1);
        upper.y = min(gridDims.y - 1, gridPosition.y + 1);
        upper.z = min(gridDims.z - 1, gridPosition.z + 1);

        int numCells = gridDims.x * gridDims.y * gridDims.z;
        glm::vec3 posI = position;
        glm::vec3 velI = velocities[idx];
        float pressureI = pressures[idx];
        float densityI  = densities[idx];

        glm::vec3 ni;
        int nbrs = 0;
        glm::vec3 fPressure(0.0f);
        glm::vec3 fVisco(0.0f);
        for (int z = lower.z; z <= upper.z; ++z)
        {
            for (int y = lower.y; y <= upper.y; ++y)
            {
                for (int x = lower.x; x <= upper.x; ++x)
                {
                    int linearCellIdx = getGridLinearIndex(x, y, z, gridDims);
                    int cellOffset = gridOffsets[linearCellIdx];
                    int nextCellOffset = linearCellIdx == numCells - 1 ? numParticles : gridOffsets[linearCellIdx + 1];
                    for (int i = cellOffset; i < nextCellOffset; ++i)
                    {
                        int j = particleIndices[i];
                        glm::vec3 posJ = positions[j];

                        glm::vec3 diff = posI - posJ;
                        float dist = glm::length(diff);

                        if (dist > 0.0f && dist < h)
                        {
                            float volJ = mass / densities[j];

                            fPressure += -volJ * (pressureI + pressures[j]) * 0.5f  * spikyGrad(diff, dist);
                            fVisco    +=  volJ * (velocities[j] - velI) * viscoLaplacian(dist);
                            ni += volJ * spikyGrad(diff, dist);
                        }
                        nbrs++;
                    }
                }
            }
        }
        
        glm::vec3 fGrav = densityI * gravity;

        glm::vec3 total = fPressure + viscosity * fVisco + fGrav;
        
        forces[idx] = total;

        glm::vec3 col = glm::normalize(total) * 0.5f + 0.5f;
        colors[idx] = glm::vec4(posI.y, 1.5f * posI.y, 1.0f, 1.0f);
        //colors[idx] = glm::vec4(glm::normalize(velI) * 0.5f + 0.5f, 1.0f);
        //colors[idx] = glm::vec4(glm::vec3(densityI / 1000.0f), 1);
        if (densityI < restDensity)
            colors[idx] = glm::vec4((restDensity - densityI) / 500.0f + 0.5f, 0.5f, 0.5f, 1.0f);
        else
            colors[idx] = glm::vec4(0.5f, 0.5f, (densityI - restDensity) / 500.0f + 0.5f, 1.0f);

        float norm = float(nbrs) / 255;
        //colors[idx] = glm::vec4(getHeatMapColor(float(nbrs)), 1.0f);
        colors[idx] = glm::vec4(glm::vec3(glm::length(ni * h)), 1.0f);
    }

    void FluidSimulationKernels::computeForces(PropertyBuffer<glm::vec3>& forces, PropertyBuffer<glm::vec4>& colors,
        const PropertyBuffer<glm::vec4>& positions, const PropertyBuffer<int>& gridOffsets, const PropertyBuffer<int>& gridLocations, const PropertyBuffer<int>& particleIndices,
        const PropertyBuffer<float>& densities, const PropertyBuffer<float>& pressures, const PropertyBuffer<glm::vec3>& velocities, const PropertyBuffer<glm::vec4>& normals,
        int3 gridDims, float cellSize, const glm::vec3& gravity, float viscosity)
    {
        int numParticles = static_cast<int>(positions.getSize());
        int blockSize = 256;
        int gridSize = (numParticles - 1) / blockSize + 1;
        computeForcesKernel<<<gridSize, blockSize>>>(forces.getDeviceBuffer(), colors.getDeviceBuffer(),
            positions.getDeviceBuffer(), gridOffsets.getDeviceBuffer(), gridLocations.getDeviceBuffer(), particleIndices.getDeviceBuffer(),
            densities.getDeviceBuffer(), pressures.getDeviceBuffer(), velocities.getDeviceBuffer(), normals.getDeviceBuffer(), numParticles, gridDims, cellSize, gravity, viscosity);
    }

    __global__ void integrateKernel(glm::vec4* positions, glm::vec3* velocities, glm::vec3* forces, float* densities, int numParticles, float3 fluidSpace, float timeStep)
    {
        int idx = blockIdx.x * blockDim.x + threadIdx.x;
        if (idx >= numParticles)
            return;

        float step = timeStep;

        glm::vec3 a = forces[idx] / densities[idx];
        glm::vec3 velocity = velocities[idx] + step * a;

        glm::vec4 position = positions[idx] + glm::vec4(step * velocity, 0.0f);

        constexpr float damping = 0.5f;

        if (position.x < particleRadius)
        {
            position.x = particleRadius;
            velocity.x = damping * -velocity.x;
        }

        if (position.x > fluidSpace.x - particleRadius)
        {
            position.x = fluidSpace.x - particleRadius;
            velocity.x = damping * -velocity.x;
        }

        if (position.y < particleRadius)
        {
            position.y = particleRadius;
            velocity.y = damping * -velocity.y;
        }

        if (position.y > fluidSpace.y - particleRadius)
        {
            position.y = fluidSpace.y - particleRadius;
            velocity.y = damping * -velocity.y;
        }

        if (position.z < particleRadius)
        {
            position.z = particleRadius;
            velocity.z = damping * -velocity.z;
        }

        if (position.z > fluidSpace.z - particleRadius)
        {
            position.z = fluidSpace.z - particleRadius;
            velocity.z = damping * -velocity.z;
        }

        velocities[idx] = velocity;
        positions[idx]  = position;
    }

    void FluidSimulationKernels::integrate(PropertyBuffer<glm::vec4>& pos, PropertyBuffer<glm::vec3>& vel,
        const PropertyBuffer<glm::vec3>& forces, const PropertyBuffer<float>& densities, float3 fluidSpace, float timeStep)
    {
        int numParticles = static_cast<int>(pos.getSize());
        int blockSize = 256;
        int gridSize = (numParticles - 1) / blockSize + 1;
        integrateKernel<<<gridSize, blockSize>>>(pos.getDeviceBuffer(), vel.getDeviceBuffer(), forces.getDeviceBuffer(), densities.getDeviceBuffer(), numParticles, fluidSpace, timeStep);
    }
}