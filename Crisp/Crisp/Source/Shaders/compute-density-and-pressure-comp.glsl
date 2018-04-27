#version 450 core

// Input buffers
// ==========================================
// Particle positions
layout(set = 0, binding = 0) buffer Positions
{
    vec4 positions[];
};

// Prefix sum of cell counts
layout(set = 0, binding = 1) buffer CellCounts
{
    uint cellCounts[];
};

layout(set = 0, binding = 2) buffer ReorderedPositions
{
   vec4 reorderedPositions[];
};
// layout(set = 0, binding = 2) buffer Indices
// {
//     uint indices[];
// };

// Output buffers
// ==========================================
// Particle densities
layout(set = 0, binding = 3) buffer Densities
{
    float densities[];
};

// Particle pressures
layout(set = 0, binding = 4) buffer Pressures
{
    float pressures[];
};

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(push_constant) uniform PushConstant
{
    layout(offset = 0)  uvec3 dim;
    layout(offset = 12) uint  numCells;
    layout(offset = 16) vec3  spaceSize;
    layout(offset = 28) float cellSize;
    layout(offset = 32) uint numParticles;
} pc;

const float PI = 3.1415926535897932384626433832795f;
const float particleRadius = 0.01f;
const float particleDiameter = 2.0f * particleRadius;
const float particleVolume = 1.33f * PI * particleRadius * particleRadius * particleRadius;
const float restDensity = 1000.0f;
const float mass = 1.3 * particleVolume * restDensity;
const float stiffness = 100.0f;

const float h = 4.0f * particleRadius;
const float h2 = h * h;
const float h3 = h2 * h;

const float poly6Const = 315.0f / (64.0f * PI * h3 * h3 * h3);

uint getGridLinearIndex(uvec3 gridPosition, uvec3 gridDims)
{
    return gridPosition.z * gridDims.x * gridDims.y +
           gridPosition.y * gridDims.x +
           gridPosition.x;
}

uint getGlobalIndex()
{
    return getGridLinearIndex(gl_GlobalInvocationID, gl_WorkGroupSize * gl_NumWorkGroups);
}

float poly6(float x)
{
    if (x >= h)
        return 0.0f;

    float val = h2 - x * x;
    return poly6Const * val * val * val;
}

ivec3 calculateGridPosition(vec3 position, float cellSize)
{
    ivec3 gridPosition;
    gridPosition.x = int(position.x / cellSize);
    gridPosition.y = int(position.y / cellSize);
    gridPosition.z = int(position.z / cellSize);
    return gridPosition;
}

void main()
{
    uint threadIdx = getGlobalIndex();
    uint numParticles = pc.numParticles;
    if (threadIdx >= numParticles)
        return;

    vec3 position = positions[threadIdx].xyz;
    ivec3 gridPosition = calculateGridPosition(position, pc.cellSize);

    ivec3 lo = max(ivec3(0), gridPosition - ivec3(1));
    ivec3 hi = min(ivec3(pc.dim) - ivec3(1), gridPosition + ivec3(1));

    float density = 0.0f;
    for (uint cellZ = lo.z; cellZ <= hi.z; cellZ++)
    {
       for (uint cellY = lo.y; cellY <= hi.y; cellY++)
       {
           for (uint cellX = lo.x; cellX <= hi.x; cellX++)
           {
                uint cellIdx = getGridLinearIndex(uvec3(cellX, cellY, cellZ), pc.dim);
                uint cellStart = cellCounts[cellIdx];
                uint cellEnd   = cellIdx == pc.numCells - 1 ? numParticles : cellCounts[cellIdx + 1];
                for (uint k = cellStart; k < cellEnd; k++)
                {
                    vec3 posJ = reorderedPositions[k].xyz;

                    float dist = length(position - posJ);
                    density += mass * poly6(dist);
                }
           }
       }
    }

    densities[threadIdx] = density;
    pressures[threadIdx] = max(0.0f, stiffness * (density - restDensity));
}