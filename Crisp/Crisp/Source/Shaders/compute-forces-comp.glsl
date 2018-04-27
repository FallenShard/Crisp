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

// Particles belonging to cell i are identified with
// indices[cellCounts[i]]...indices[cellCounts[i + 1]]
layout(set = 0, binding = 2) buffer Indices
{
    uint indices[];
};

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

// Particle velocities
layout(set = 0, binding = 5) buffer Velocities
{
    vec4 velocities[];
};

// Output buffers
// ==========================================
// Particle densities
layout(set = 0, binding = 6) buffer Forces
{
    vec4 forces[];
};

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(push_constant) uniform PushConstant
{
    layout(offset = 0)  uvec3 dim;
    layout(offset = 12) uint  numCells;
    layout(offset = 16) vec3  spaceSize;
    layout(offset = 28) float cellSize;
    layout(offset = 32) vec3 gravity;
    layout(offset = 44) uint numParticles;
    layout(offset = 48) float viscosity;
    layout(offset = 52) float kappa;
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
const float spikyGradConst = -45.0f / (PI * h2 * h2 * h2);
const float viscosityLaplaceConst = 45.0f / (PI * h3 * h3);

uint getGlobalIndex()
{
    uvec3 dim = gl_WorkGroupSize * gl_NumWorkGroups;
    return gl_GlobalInvocationID.z * dim.x * dim.y +
           gl_GlobalInvocationID.y * dim.x +
           gl_GlobalInvocationID.x;
}

float cubicSpline(float x)
{
    if (x >= h)
        return 0.0f;

    float q = x / h;
    const float c = 3.0f / (2.0f * PI);
    if (q > 0.5f)
    {
        float a = (2 - q);
        return 1.0f / 6.0f * a * a * a;
    }
    else
    {
        float q2 = q * q;
        return 2.0f / 3.0f - q2 + 0.5f * q2 * q;
    }
}

float poly6(float x)
{
    if (x >= h)
        return 0.0f;
                
    float val = h2 - x * x;
    return poly6Const * val * val * val;
}

vec3 spikyGrad(vec3 v, float x)
{
    return spikyGradConst * (h - x) * (h - x) * (1.0f / x) * v;
}

float viscoLaplacian(float x)
{
    return viscosityLaplaceConst * (h - x);
}

ivec3 calculateGridPosition(vec3 position, float cellSize)
{
    ivec3 gridPosition;
    gridPosition.x = int(position.x / cellSize);
    gridPosition.y = int(position.y / cellSize);
    gridPosition.z = int(position.z / cellSize);
    return gridPosition;
}

uint getGridLinearIndex(uvec3 gridPosition, uvec3 gridDims)
{
    return gridPosition.z * gridDims.x * gridDims.y +
           gridPosition.y * gridDims.x +
           gridPosition.x;
}

void main()
{
    uint threadIdx = getGlobalIndex();
    uint numParticles = pc.numParticles;
    if (threadIdx >= numParticles)
        return;

    vec3 position = positions[threadIdx].xyz;
    
    ivec3 gridPosition = calculateGridPosition(position, pc.cellSize);
    ivec3 lo = max(ivec3(0),                 gridPosition - ivec3(1));
    ivec3 hi = min(ivec3(pc.dim) - ivec3(1), gridPosition + ivec3(1));

    vec3 velI = velocities[threadIdx].xyz;
    float pressureI = pressures[threadIdx];
    float densityI  = densities[threadIdx];

    vec3 fPressure  = vec3(0.0f);
    vec3 fViscosity = vec3(0.0f);
    vec3 fTension   = vec3(0.0f);
    for (uint cellZ = lo.z; cellZ <= hi.z; cellZ++)
    {
       for (uint cellY = lo.y; cellY <= hi.y; cellY++)
       {
           for (uint cellX = lo.x; cellX <= hi.x; cellX++)
           {
                uint cellIdx = getGridLinearIndex(uvec3(cellX, cellY, cellZ), pc.dim);
                uint cellStart = cellCounts[cellIdx];
                uint cellEnd   = cellIdx == pc.numCells - 1 ? pc.numParticles : cellCounts[cellIdx + 1];
                for (uint k = cellStart; k < cellEnd; k++)
                {
                    uint j = indices[k];
                    vec3 posJ = positions[j].xyz;

                    vec3 diff = position - posJ;
                    float dist2 = dot(diff, diff);

                    if (dist2 > 0.0f && dist2 < h2)
                    {
                        float volJ = mass / densities[j];
                        float dist = sqrt(dist2);

                        fPressure  += -volJ * (pressureI + pressures[j]) * 0.5f * spikyGrad(diff, dist);
                        fViscosity += +volJ * (velocities[j].xyz - velI) * viscoLaplacian(dist);

                        fTension += -cubicSpline(dist) * diff / dist;
                    }
                }
           }
       }
    }

    vec3 fGravity = densityI * pc.gravity;
    vec3 fTotal = fPressure + pc.viscosity * fViscosity + fGravity + densityI * pc.kappa * fTension;

    forces[threadIdx] = vec4(fTotal, densityI);
}