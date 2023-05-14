#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require

#define PI 3.1415926535897932384626433832795
#define InvPI 0.31830988618379067154
#define InvTwoPI 0.15915494309189533577f

struct HitInfo
{
    vec3 position;
    float tHit;

    vec3 sampleDirection;
    float samplePdf;

    vec3 Le;
    uint bounceCount;

    vec3 bsdfEval;
    uint rngSeed;

    vec4 debugValue;
};

struct BsdfSample
{
    vec2 unitSample;
    vec2 pad0;

    vec3 normal;
    float pad1;

    vec3 sampleDirection;
    float samplePdf;

    vec3 eval;
    float pad2;
};

layout(location = 0) rayPayloadInEXT HitInfo hitInfo;
layout(location = 1) callableDataEXT BsdfSample bsdfSample;

const uint kLambertianShaderCallable = 0;

hitAttributeEXT vec2 barycentric;

layout(set = 1, binding = 0, scalar) buffer Vertices
{
    float v[];
} vertices[4];
layout(set = 1, binding = 1, scalar) buffer Indices
{
    uint i[];
} indices[4];

uint xorshift32(inout uint state)
{
	/* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
	uint x = state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
    state = x;
	return x;
}

float rndFloat(inout uint state)
{
    const uint one = 0x3f800000;
    const uint msk = 0x007fffff;
    return uintBitsToFloat(one | (msk & (xorshift32(state) >> 9))) - 1.0f;
}

vec3 squareToCosineHemisphere(vec2 unitSample)
{
    vec3 dir;
    float radius = sqrt(unitSample.y);
    float theta = 2.0f * PI * unitSample.x;
    dir.x = radius * cos(theta);
    dir.y = radius * sin(theta);
    dir.z = sqrt(max(0.0f, 1.0f - dir.x * dir.x - dir.y  * dir.y ));
    return dir;
}

float squareToCosineHemispherePdf(vec3 sampleDir)
{
    return sampleDir.z * InvPI;
}

vec3 squareToUniformCylinder(const vec2 unitSample, float cosThetaMin, float cosThetaMax)
{
    float z = cosThetaMin + unitSample.y * (cosThetaMax - cosThetaMin);
    float phi = 2.0f * PI * unitSample.x;
    return vec3(cos(phi), sin(phi), z);
}

vec3 cylinderToSphereSection(const vec2 unitSample, float cosThetaMin, float cosThetaMax)
{
    vec3 cylPt = squareToUniformCylinder(unitSample, cosThetaMin, cosThetaMax);
    cylPt.xy *= sqrt(1.0f - cylPt.z * cylPt.z);
    return cylPt;
}

vec3 squareToUniformHemisphere(const vec2 unitSample)
{
    return cylinderToSphereSection(unitSample, 1.0f, 0.0f);
}

vec3 squareToUniformSphere(const vec2 unitSample)
{
    return cylinderToSphereSection(unitSample, 1.0f, -1.0f);
}

vec3 evalAreaLight(vec3 p, vec3 n)
{
    const vec3 ref = gl_WorldRayOriginEXT;
    const vec3 wi = p - ref;
    const float cosTheta = dot(n, normalize(-wi));
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

layout(push_constant) uniform PushConstant
{
    uint frameIdx;
} pushConst;

const int kVertexComponentCount = 6;
const int kPositionComponentOffset = 0;
const int kNormalComponentOffset = 3;

vec3 getNormal(const uint objectId, const ivec3 ind, const vec3 bary)
{
    vec3 n0 = vec3(
        vertices[objectId].v[kVertexComponentCount * ind.x + kNormalComponentOffset],
        vertices[objectId].v[kVertexComponentCount * ind.x + kNormalComponentOffset + 1],
        vertices[objectId].v[kVertexComponentCount * ind.x + kNormalComponentOffset + 2]);
    vec3 n1 = vec3(
        vertices[objectId].v[kVertexComponentCount * ind.y + kNormalComponentOffset],
        vertices[objectId].v[kVertexComponentCount * ind.y + kNormalComponentOffset + 1],
        vertices[objectId].v[kVertexComponentCount * ind.y + kNormalComponentOffset + 2]);
    vec3 n2 = vec3(
        vertices[objectId].v[kVertexComponentCount * ind.z + kNormalComponentOffset],
        vertices[objectId].v[kVertexComponentCount * ind.z + kNormalComponentOffset + 1],
        vertices[objectId].v[kVertexComponentCount * ind.z + kNormalComponentOffset + 2]);
    return normalize(n0 * bary.x + n1 * bary.y + n2 * bary.z);
}

vec3 getPosition(uint objectId, ivec3 ind, vec3 bary)
{
    vec3 p0 = vec3(
        vertices[objectId].v[kVertexComponentCount * ind.x + kPositionComponentOffset],
        vertices[objectId].v[kVertexComponentCount * ind.x + kPositionComponentOffset + 1],
        vertices[objectId].v[kVertexComponentCount * ind.x + kPositionComponentOffset + 2]);
    vec3 p1 = vec3(
        vertices[objectId].v[kVertexComponentCount * ind.y + kPositionComponentOffset],
        vertices[objectId].v[kVertexComponentCount * ind.y + kPositionComponentOffset + 1],
        vertices[objectId].v[kVertexComponentCount * ind.y + kPositionComponentOffset + 2]);
    vec3 p2 = vec3(
        vertices[objectId].v[kVertexComponentCount * ind.z + kPositionComponentOffset],
        vertices[objectId].v[kVertexComponentCount * ind.z + kPositionComponentOffset + 1],
        vertices[objectId].v[kVertexComponentCount * ind.z + kPositionComponentOffset + 2]);
    return p0 * bary.x + p1 * bary.y + p2 * bary.z;
}

float fresnelDielectric(float cosThetaI, float extIOR, float intIOR, inout float cosThetaT)
{
    float etaI = extIOR, etaT = intIOR;

    // If indices of refraction are the same, no fresnel effects.
    if (extIOR == intIOR)
        return 0.0f;

    // if cosThetaI is < 0, it means the ray is coming from inside the object.
    if (cosThetaI < 0.0f)
    {
        float temp = etaI;
        etaI = etaT;
        etaT = temp;
        cosThetaI = -cosThetaI;
    }

    float eta = etaI / etaT;
    float sinThetaTSqr = eta * eta * (1.0f - cosThetaI * cosThetaI);

    // Total internal reflection.
    if (sinThetaTSqr > 1.0f)
        return 1.0f;

    cosThetaT = sqrt(1.0f - sinThetaTSqr);

    float Rs = (etaI * cosThetaI - etaT * cosThetaT) / (etaI * cosThetaI + etaT * cosThetaT);
    float Rp = (etaT * cosThetaI - etaI * cosThetaT) / (etaT * cosThetaI + etaI * cosThetaT);
    return (Rs * Rs + Rp * Rp) / 2.0f;
}

void sampleLambertianBrdf(in vec3 albedo, in vec3 normal, in vec2 unitSample)
{
    const vec3 cwhSample = squareToCosineHemisphere(unitSample);

    const vec3 upVector = abs(normal.y) < 0.999 ? vec3(0, 1, 0) : vec3(0, 0, 1);
    const vec3 tangentX = normalize(cross(upVector, normal));
    const vec3 tangentY = cross(normal, tangentX);

    hitInfo.sampleDirection = tangentX * cwhSample.x + tangentY * cwhSample.y + normal * cwhSample.z;
    hitInfo.samplePdf = InvTwoPI;
    hitInfo.bsdfEval = albedo;
}

void sampleMirrorBrdf(in vec3 normal)
{
    hitInfo.sampleDirection = reflect(gl_WorldRayDirectionEXT, normal);
    hitInfo.samplePdf = 0.0f;
    hitInfo.bsdfEval = vec3(1.0f);
}

void sampleDielectricBrdf(in vec3 normal, in float unitSample)
{
    const float etaRatio = 1.33157 / 1.00029;
    const float cosThetaI = dot(normal, -gl_WorldRayDirectionEXT);
    const vec3 localNormal = cosThetaI < 0.0f ? -normal : normal;
    const float eta        = cosThetaI < 0.0f ? etaRatio : 1.0f / etaRatio;
    const float cosine     = cosThetaI < 0.0f ? etaRatio * cosThetaI : -cosThetaI;
    float cosThetaT = 0.0f;
    float fresnel = fresnelDielectric(cosThetaI, 1.00029f, 1.33157, cosThetaT);

    if (unitSample <= fresnel)
    {
        hitInfo.sampleDirection = reflect(gl_WorldRayDirectionEXT, localNormal);
        hitInfo.samplePdf = fresnel;
        hitInfo.bsdfEval = vec3(1.0f);
    }
    else
    {
        hitInfo.sampleDirection = refract(gl_WorldRayDirectionEXT, localNormal, eta);
        hitInfo.samplePdf = 1.0f - fresnel;
        hitInfo.bsdfEval = vec3(eta * eta);
    }
}

vec3 getAlbedo(const uint objId)
{
    if (objId == 0)
        return vec3(0.725, 0.71, 0.68); // Top, bottom, back wall.
    if (objId == 1)
        return vec3(0.630, 0.065, 0.05); // Left wall.
    if (objId == 2)
        return vec3(0.161, 0.133, 0.427); // Right wall.
    if (objId == 3)
        return vec3(0.5f); // Area light.
    
    return vec3(1.0f);
}

void main()
{
    // Grab the ID of the object that we just hit.
    const uint objId = gl_InstanceCustomIndexEXT;

    // Formulate the triangle at the hit.
    const ivec3 hitTriangle = ivec3(
        indices[objId].i[3 * gl_PrimitiveID + 0],
        indices[objId].i[3 * gl_PrimitiveID + 1],
        indices[objId].i[3 * gl_PrimitiveID + 2]);

    const vec3 baryCoord = vec3(1.0 - barycentric.x - barycentric.y, barycentric.x, barycentric.y);
    const vec3 normal   = getNormal(objId, hitTriangle, baryCoord);
    const vec3 position = getPosition(objId, hitTriangle, baryCoord);

    // Record the hit info for the calling shader.
    hitInfo.position = position;
    hitInfo.tHit = gl_HitTEXT;

    // Determine sampled BRDF and the new path direction.
    if (objId <= 3)
    {
        const float r1 = rndFloat(hitInfo.rngSeed);
        const float r2 = rndFloat(hitInfo.rngSeed);

        bsdfSample.unitSample = vec2(r1, r2);
        bsdfSample.normal = normal;
        executeCallableEXT(kLambertianShaderCallable, 1);
        hitInfo.sampleDirection = bsdfSample.sampleDirection;
        hitInfo.samplePdf = bsdfSample.samplePdf;
        hitInfo.bsdfEval = getAlbedo(objId);
    }
    else if (objId == 4)
    {
        sampleMirrorBrdf(normal);
    }
    else if (objId == 5)
    {
        sampleDielectricBrdf(normal, rndFloat(hitInfo.rngSeed));
    }

    // Account for any lights hit.    
    hitInfo.Le = vec3(0.0f);
    if (objId == 3)
    {
        hitInfo.Le = evalAreaLight(position, normal);// eval light
    }
}
