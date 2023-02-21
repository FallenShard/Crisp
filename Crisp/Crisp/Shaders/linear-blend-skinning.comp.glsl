#version 450 core

layout(set = 0, binding = 0) buffer RestPositions
{
    float restPositions[];
};

layout(set = 0, binding = 1) buffer Weights
{
    float weights[];
};

layout(set = 0, binding = 2) buffer Indices
{
    uint indices[];
};

layout(set = 0, binding = 3) buffer JointMatrices
{
    mat4 jointMatrices[];
};

layout(set = 0, binding = 4) buffer Positions
{
    float positions[];
};

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(push_constant) uniform PushConstant
{
    layout(offset = 0) uint vertexCount;
    layout(offset = 4) uint jointsPerVertex;
} pushConst;

vec3 getRestPosition(uint index)
{
    return vec3(restPositions[3 * index], restPositions[3 * index + 1], restPositions[3 * index + 2]);
}

void setPosition(uint index, vec3 pos)
{
    positions[3 * index] = pos.x;
    positions[3 * index + 1] = pos.y;
    positions[3 * index + 2] = pos.z;
}

void main()
{
    const uint threadIdx = gl_GlobalInvocationID.x;
    if (threadIdx >= pushConst.vertexCount)
        return;

    vec3 restPosition = getRestPosition(threadIdx);
    vec4 skinnedPosition = vec4(0.0f);
    for (uint i = 0; i < pushConst.jointsPerVertex; ++i)
    {
        const float w = weights[threadIdx * pushConst.jointsPerVertex + i];
        const uint idx = indices[threadIdx * pushConst.jointsPerVertex + i];
        skinnedPosition += w * jointMatrices[idx] * vec4(restPosition, 1.0f);
    }

    setPosition(threadIdx, skinnedPosition.xyz);
}