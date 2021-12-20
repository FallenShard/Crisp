#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

struct hitPayload
{
    vec3 hitPos;
    float tHit;
    vec3 sampleDir;
    float pdf;
    vec3 Le;
    uint bounceCount;
    vec3 bsdf;
    float pad2;
    vec4 debugValue;
};

layout(location = 0) rayPayloadInEXT hitPayload prd;
hitAttributeEXT vec2 attribs;

layout(binding = 0, set = 1, scalar) buffer Vertices
{
    float v[];
} vertices[4];
layout(binding = 1, set = 1, scalar) buffer Indices
{
    uint i[];
} indices[4];

layout(binding = 2, set = 1, scalar) buffer RandomBuffer
{
    float vals[];
} randBuffer;

#define PI 3.1415926535897932384626433832795
#define InvPI 0.31830988618379067154
#define InvTwoPI 0.15915494309189533577f

uint tea(uint val0, uint val1)
{
  uint v0 = val0;
  uint v1 = val1;
  uint s0 = 0;

  for(uint n = 0; n < 16; n++)
  {
    s0 += 0x9e3779b9;
    v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
    v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
  }

  return v0;
}

// Generate a random unsigned int in [0, 2^24) given the previous RNG state
// using the Numerical Recipes linear congruential generator
uint lcg(inout uint prev)
{
  uint LCG_A = 1664525u;
  uint LCG_C = 1013904223u;
  prev       = (LCG_A * prev + LCG_C);
  return prev & 0x00FFFFFF;
}

uint RandomInt(inout uint seed)
{
    // LCG values from Numerical Recipes
    return (seed = 1664525 * seed + 1013904223);
}

float RandomFloat(inout uint seed)
{
    // Float version using bitmask from Numerical Recipes
    const uint one = 0x3f800000;
    const uint msk = 0x007fffff;
    return uintBitsToFloat(one | (msk & (RandomInt(seed) >> 9))) - 1;
}

// Generate a random float in [0, 1) given the previous RNG state
float rnd(inout uint prev)
{
  return (float(lcg(prev)) / float(0x01000000));
}

vec3 squareToCosineHemisphere(vec2 randomSample)
{
    vec3 dir;
    float radius = sqrt(randomSample.y);
    float theta = 2.0f * PI * randomSample.x;
    dir.x = radius * cos(theta);
    dir.y = radius * sin(theta);
    dir.z = sqrt(max(0.0f, 1.0f - dir.x * dir.x - dir.y  * dir.y ));
    return dir;
}

float squareToCosineHemispherePdf(vec3 sampleDir)
{
    return sampleDir.z * InvPI;
}

vec3 squareToUniformCylinder(const vec2 randomSample, float cosThetaMin, float cosThetaMax)
{
    float z = cosThetaMin + randomSample.y * (cosThetaMax - cosThetaMin);
    float phi = 2.0f * PI * randomSample.x;
    return vec3(cos(phi), sin(phi), z);
}

vec3 cylinderToSphereSection(const vec2 randomSample, float cosThetaMin, float cosThetaMax)
{
    vec3 cylPt = squareToUniformCylinder(randomSample, cosThetaMin, cosThetaMax);
    cylPt.xy *= sqrt(1.0f - cylPt.z * cylPt.z);
    return cylPt;
}

vec3 squareToUniformHemisphere(const vec2 randomSample)
{
    return cylinderToSphereSection(randomSample, 1.0f, 0.0f);
}

vec3 squareToUniformSphere(const vec2 randomSample)
{
    return cylinderToSphereSection(randomSample, 1.0f, -1.0f);
}

vec3 RandomInUnitSphere(inout uint seed, vec3 normal)
{
    for (;;)
    {
        float eps1 = RandomFloat(seed);
        float eps2 = RandomFloat(seed);
        //const vec3 p = squareToCosineHemisphere(vec2(eps1, eps2));
        const vec3 p = 2 * vec3(eps1, eps2, RandomFloat(seed)) - 1;
        if (dot(p, p) <= 1 && dot(p, normal) >= 0)
        {
            return normalize(p);
        }
    }
}

vec3 MyRandomHemisphere(inout uint seed, vec3 normal)
{
    for (;;)
    {
        float eps1 = RandomFloat(seed);
        float eps2 = RandomFloat(seed);
        const vec3 p = squareToUniformSphere(vec2(eps1, eps2));
        if (dot(p, normal) >= 0.0f)
        {
            return p;
        }
    }
}



vec3 evalAreaLight(vec3 p, vec3 n)
{
    vec3 ref = gl_WorldRayOriginEXT;
    vec3 wi = p - ref;
    float cosTheta = dot(n, normalize(-wi));

    //return gl_WorldRayOriginNV;
    //return vec3(abs(p));

    return cosTheta <= 0.0f ? vec3(0.0f) : vec3(15.0f);
}

void coordinateSystem(in vec3 v1, out vec3 v2, out vec3 v3)
{
    if (abs(v1.x) > abs(v1.y))
    {
        float invLen = 1.0f / sqrt(v1.x * v1.x + v1.z * v1.z);
        v3 = vec3(v1.z * invLen, 0.0f, -v1.x * invLen);
    }
    else
    {
        float invLen = 1.0f / sqrt(v1.y * v1.y + v1.z * v1.z);
        v3 = vec3(0.0f, v1.z * invLen, -v1.y * invLen);
    }
    v2 = cross(v3, v1);
}

void coordinateSystem2(in vec3 v1, out vec3 v2, out vec3 v3)
{
    vec3 ref = vec3(0.0, 0.9, 0.1);
    v3 = cross(v1, ref);
    v2 = cross(v3, v1);
}

vec3 transformToWorld(vec3 cs, vec3 ct, vec3 cn, vec3 vec)
{
    return cs * vec.x + ct * vec.y + cn * vec.z;
}

layout(push_constant) uniform PushConstant
{
    uint frameIdx;
} pushConst;


vec3 getNormal(uint objectId, ivec3 ind, vec3 bary)
{
    const int normalOffset = 3;
    const int vertexComponents = 6;
    vec3 n0 = vec3(vertices[objectId].v[vertexComponents * ind.x + normalOffset],
        vertices[objectId].v[vertexComponents * ind.x + normalOffset + 1],
        vertices[objectId].v[vertexComponents * ind.x + normalOffset + 2]);
    vec3 n1 = vec3(vertices[objectId].v[vertexComponents * ind.y + normalOffset],
        vertices[objectId].v[vertexComponents * ind.y + normalOffset + 1],
        vertices[objectId].v[vertexComponents * ind.y + normalOffset + 2]);
    vec3 n2 = vec3(vertices[objectId].v[vertexComponents * ind.z + normalOffset],
        vertices[objectId].v[vertexComponents * ind.z + normalOffset + 1],
        vertices[objectId].v[vertexComponents * ind.z + normalOffset + 2]);
    return normalize(n0 * bary.x + n1 * bary.y + n2 * bary.z);
}

vec3 getPosition(uint objectId, ivec3 ind, vec3 bary)
{
    const int positionOffset = 0;
    const int vertexComponents = 6;
    vec3 p0 = vec3(vertices[objectId].v[vertexComponents * ind.x + positionOffset],
        vertices[objectId].v[vertexComponents * ind.x + positionOffset + 1],
        vertices[objectId].v[vertexComponents * ind.x + positionOffset + 2]);
    vec3 p1 = vec3(vertices[objectId].v[vertexComponents * ind.y + positionOffset],
        vertices[objectId].v[vertexComponents * ind.y + positionOffset + 1],
        vertices[objectId].v[vertexComponents * ind.y + positionOffset + 2]);
    vec3 p2 = vec3(vertices[objectId].v[vertexComponents * ind.z + positionOffset],
        vertices[objectId].v[vertexComponents * ind.z + positionOffset + 1],
        vertices[objectId].v[vertexComponents * ind.z + positionOffset + 2]);
    return p0 * bary.x + p1 * bary.y + p2 * bary.z;
}

float fresnelDielectric(float cosThetaI, float extIOR, float intIOR, inout float cosThetaT)
{
    float etaI = extIOR, etaT = intIOR;

    // If indices of refraction are the same, no fresnel effects
    if (extIOR == intIOR)
        return 0.0f;

    // if cosThetaI is < 0, it means the ray is coming from inside the object
    if (cosThetaI < 0.0f)
    {
        float temp = etaI;
        etaI = etaT;
        etaT = temp;
        cosThetaI = -cosThetaI;
    }

    float eta = etaI / etaT;
    float sinThetaTSqr = eta * eta * (1.0f - cosThetaI * cosThetaI);

    // Total internal reflection
    if (sinThetaTSqr > 1.0f)
        return 1.0f;

    cosThetaT = sqrt(1.0f - sinThetaTSqr);

    float Rs = (etaI * cosThetaI - etaT * cosThetaT) / (etaI * cosThetaI + etaT * cosThetaT);
    float Rp = (etaT * cosThetaI - etaI * cosThetaT) / (etaT * cosThetaI + etaI * cosThetaT);
    return (Rs * Rs + Rp * Rp) / 2.0f;
}

float Schlick(const float cosine, const float refractionIndex)
{
    float r0 = (1 - refractionIndex) / (1 + refractionIndex);
    r0 *= r0;
    return r0 + (1 - r0) * pow(1 - cosine, 5);
}

void main()
{
    // Object of this instance
    uint objId = gl_InstanceCustomIndexEXT;//scnDesc.i[gl_InstanceID].objId;

    // Indices of the triangle
    ivec3 ind = ivec3(indices[objId].i[3 * gl_PrimitiveID + 0],   //
                      indices[objId].i[3 * gl_PrimitiveID + 1],   //
                      indices[objId].i[3 * gl_PrimitiveID + 2]);  //
    // Vertex of the triangle
    //Vertex v0 = vertices[nonuniformEXT(objId)].v[ind.x];
    //Vertex v1 = vertices[nonuniformEXT(objId)].v[ind.y];
    //Vertex v2 = vertices[nonuniformEXT(objId)].v[ind.z];

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 normal   = getNormal(objId, ind, barycentrics);
    const vec3 position = getPosition(objId, ind, barycentrics);

    

    

    vec3 albedo = vec3(0.0f);
    if (objId == 0) // main wall
        albedo = vec3(0.725, 0.71, 0.68);
    else if (objId == 1) // left wall
        albedo = vec3(0.630, 0.065, 0.05);
    else if (objId == 2) // right wall
        albedo = vec3(0.161, 0.133, 0.427);
    else if (objId == 3) // area light
        albedo = vec3(0.5f);

    prd.hitPos = position;
    prd.tHit = gl_HitTEXT;

    uint seed = tea(gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x, 1024 * pushConst.frameIdx + prd.bounceCount);
    
    if (objId <= 3)
    {
        float r1 = RandomFloat(seed);
        float r2 = RandomFloat(seed);
        vec3 sampledDir = squareToCosineHemisphere(vec2(r1, r2));

        vec3 UpVector = abs(normal.y) < 0.999 ? vec3(0, 1, 0) : vec3(0, 0, 1);
        vec3 TangentX = normalize(cross(UpVector, normal));
        vec3 TangentY = cross(normal, TangentX);

        vec3 worldDir = TangentX * sampledDir.x + TangentY * sampledDir.y + normal * sampledDir.z;
        prd.sampleDir = worldDir;
        prd.pdf = InvTwoPI;//(sampleDir);
        prd.bsdf = albedo;
    }
    else if (objId == 4)
    {
        prd.sampleDir = reflect(gl_WorldRayDirectionEXT, normal);
        prd.bsdf = vec3(1.0f);
        prd.pdf = 0.0f;
    }
    else if (objId == 5)
    {
        const float etaRatio = 1.33157 / 1.00029;
        const float cosThetaI = dot(normal, -gl_WorldRayDirectionEXT);
        const vec3 localNormal = cosThetaI < 0.0f ? -normal : normal;
        const float eta        = cosThetaI < 0.0f ? etaRatio : 1.0f / etaRatio;
        const float cosine     = cosThetaI < 0.0f ? etaRatio * cosThetaI : -cosThetaI;
        float cosThetaT = 0.0f;
        float fresnel = fresnelDielectric(cosThetaI, 1.00029f, 1.33157, cosThetaT);

        float k = RandomFloat(seed);
        if (k <= fresnel)
        {
            prd.sampleDir = reflect(gl_WorldRayDirectionEXT, localNormal);
            prd.bsdf = vec3(1.0f);
            prd.pdf = fresnel;
        }
        else
        {
            prd.sampleDir = refract(gl_WorldRayDirectionEXT, localNormal, eta);
            prd.bsdf = vec3(eta * eta);
            prd.pdf = 1.0f - fresnel;
        }
    }
    
    prd.Le = vec3(0.0f);
    if (objId == 3)
    {
        prd.Le = evalAreaLight(position, normal);// eval light
    }
}
