#version 450 core

layout(set = 0, binding = 0) buffer FluidData
{
    vec4 positions[];
};

layout(set = 0, binding = 1) buffer Pressures
{
    float pressures[];
};

layout(set = 0, binding = 2) buffer GridCells
{
    uvec4 gridCells[];
};

layout(set = 0, binding = 3) buffer CellCounts
{
    uint cellCounts[];
};

layout(set = 0, binding = 4) uniform GridParams
{
    uvec3 dim;
    uint padding;
    vec3 spaceSize;
    float cellSize;
} gridParams;

layout (local_size_x = 2, local_size_y = 2, local_size_z = 2) in;

layout(push_constant) uniform PushConstant
{
	layout(offset = 0) float value;
} time;

const float particleRadius = 0.02f;
const float PI = 3.141592f;

const float wConst = 315.0f / (64 * PI);

uint getUniqueIdx(uvec3 indices, uvec3 dim)
{
    return indices.y * dim.z * dim.x + indices.z * dim.x + indices.x;
}

float kernel(float x, float h)
{
    if (x <= h)
    {
        float h2 = h * h;
        float invH2 = 1.0f / h2;
        float invH4 = invH2 * invH2;
        float invH9 = invH4 * invH4 * (1.0f / h);
        return wConst * invH9 * pow(h * h - x * x, 3);
    }
    else
        return 0.0f;
}

void main()
{
    uvec3 dim     = gl_WorkGroupSize * gl_NumWorkGroups;
    uvec3 indices = gl_WorkGroupSize * gl_WorkGroupID + gl_LocalInvocationID;
    uint index = indices.y * dim.x * dim.z + indices.z * dim.x + indices.x;

    uvec3 gridDim = gridParams.dim;
    vec3 cell = positions[index].xyz / gridParams.cellSize;
    uvec3 gridIndices = max(uvec3(0), min(gridDim - uvec3(1), uvec3(cell)));

    uvec3 curr = gridIndices;
    uvec3 lowerFrontLeft = max(uvec3(0),           curr + uvec3(-1));
    uvec3 upperBackRight = min(gridDim - uvec3(1), curr + uvec3(+1));

    uvec3 diff = upperBackRight - lowerFrontLeft;

    float volume = 4.0f / 3.0f * particleRadius * particleRadius * particleRadius * PI;
    float restDensity = 1000.0f;
    float stiffness = 1000.0f;
    float mass = volume * restDensity;
    float h = 1.5f * particleRadius;

    vec3 position = positions[index].xyz;
    float density = 0.0f;
    for (uint y = lowerFrontLeft.y; y <= upperBackRight.y; ++y)
    {
        for (uint z = lowerFrontLeft.z; z <= upperBackRight.z; ++z)
        {
            for (uint x = lowerFrontLeft.x; x <= upperBackRight.x; ++x)
            {
                uint nbrCellIndex = getUniqueIdx(uvec3(x, y, z), gridParams.dim);
                uint cellCount = cellCounts[nbrCellIndex];
                for (uint p = 0; p < cellCount; ++p) {
                    uint nbrParticleIndex = gridCells[nbrCellIndex][p];
                    vec3 nbrPosition = positions[nbrParticleIndex].xyz;

                    float dist = length(nbrPosition - position);
                    density += mass * kernel(dist, h);
                }
            }
        }
    }

    pressures[index] = stiffness * (pow(density / restDensity, 7.0f) - 1.0f);
    //positions[index].w = pressures[index];
}