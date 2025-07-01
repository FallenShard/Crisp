#version 460 core

#define PI 3.1415926535897932384626433832795

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

#include "Common/atmosphere.part.glsl"


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct SingleScatteringResult
{
    vec3 L;						// Scattered light (luminance)
    vec3 OpticalDepth;			// Optical depth (1/m)
    vec3 Transmittance;			// Transmittance in [0,1] (unitless)
    vec3 MultiScatAs1;
};

layout(set = 0, binding = 0) uniform AtmosphereParamsBlock
{
    AtmosphereParams atmosphere;
};

layout(set = 1, binding = 0, rgba16f) uniform writeonly image2D multiScatteringLut;
layout(set = 1, binding = 1) uniform sampler2D transmittanceLut;

float fromUnitToSubUvs(float u, float resolution) { return (u + 0.5f / resolution) * (resolution / (resolution + 1.0f)); }
float fromSubUvsToUnit(float u, float resolution) { return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f)); }

float getShadow(in AtmosphereParams Atmosphere, vec3 P)
{
    // // First evaluate opaque shadow
    // float4 shadowUv = mul(gShadowmapViewProjMat, float4(P + float3(0.0, 0.0, -Atmosphere.BottomRadius), 1.0));
    // //shadowUv /= shadowUv.w;	// not be needed as it is an ortho projection
    // shadowUv.x = shadowUv.x*0.5 + 0.5;
    // shadowUv.y = -shadowUv.y*0.5 + 0.5;
    // if (all(shadowUv.xyz >= 0.0) && all(shadowUv.xyz < 1.0))
    // {
    // 	return ShadowmapTexture.SampleCmpLevelZero(samplerShadow, shadowUv.xy, shadowUv.z);
    // }
    // return 1.0f;
    return 1.0f;
}

vec3 sampleTransmittanceLut(const float bottomRadius, const float topRadius, const float viewHeight, const float viewZenithCosAngle)
{
    const float horizon = sqrt(max(0.0f, topRadius * topRadius - bottomRadius * bottomRadius));
    const float rho = sqrt(max(0.0f, viewHeight * viewHeight - bottomRadius * bottomRadius));

    const float discriminant = viewHeight * viewHeight * (viewZenithCosAngle * viewZenithCosAngle - 1.0) + topRadius * topRadius;
    const float d = max(0.0, (-viewHeight * viewZenithCosAngle + sqrt(discriminant))); // Distance to atmosphere boundary

    const float d_min = topRadius - viewHeight;
    const float d_max = rho + horizon;
    const float x_mu = (d - d_min) / (d_max - d_min);
    const float x_r = rho / horizon;

    return textureLod(transmittanceLut, vec2(x_mu, x_r), 0).rgb;
}

SingleScatteringResult IntegrateScatteredLuminance(
    in vec2 ndcPos, in vec3 WorldPos, in vec3 WorldDir, in vec3 SunDir, in AtmosphereParams atmosphere,
    in bool ground, in float SampleCountIni, in float DepthBufferValue,
    in bool MieRayPhase, float tMaxMax)
{
    SingleScatteringResult result;
    result.L = vec3(0.0f);
    result.MultiScatAs1 = vec3(0.0f);

    vec3 ClipSpace = vec3(ndcPos, 1.0);

    // Compute next intersection with atmosphere or ground 
    vec3 earthO = vec3(0.0f, 0.0f, 0.0f);
    float tBottom = raySphereIntersectNearest(WorldPos, WorldDir, earthO, atmosphere.bottomRadius);
    float tTop = raySphereIntersectNearest(WorldPos, WorldDir, earthO, atmosphere.topRadius);
    float tMax = 0.0f;
    if (tBottom < 0.0f)
    {
        if (tTop < 0.0f)
        {
            tMax = 0.0f; // No intersection with earth nor atmosphere: stop right away  
            return result;
        }
        else
        {
            tMax = tTop;
        }
    }
    else
    {
        if (tTop > 0.0f)
        {
            tMax = min(tTop, tBottom);
        }
    }

    DepthBufferValue = 0.0f;
    // if (DepthBufferValue >= 0.0f)
    // {
    // 	ClipSpace.z = DepthBufferValue;
    // 	if (ClipSpace.z < 1.0f)
    // 	{
    // 		vec4 DepthBufferWorldPos = mul(gSkyInvViewProjMat, vec4(ClipSpace, 1.0));
    // 		DepthBufferWorldPos /= DepthBufferWorldPos.w;

    // 		float tDepth = length(DepthBufferWorldPos.xyz - (WorldPos + vec3(0.0, 0.0, -Atmosphere.BottomRadius))); // apply earth offset to go back to origin as top of earth mode. 
    // 		if (tDepth < tMax)
    // 		{
    // 			tMax = tDepth;
    // 		}
    // 	}
    // 	//		if (VariableSampleCount && ClipSpace.z == 1.0f)
    // 	//			return result;
    // }
    tMax = min(tMax, tMaxMax);

    // Sample count 
    float SampleCount = SampleCountIni;
    float SampleCountFloor = SampleCountIni;
    float tMaxFloor = tMax;
    float dt = tMax / SampleCount;

    // Phase functions
    const float cosTheta = dot(SunDir, WorldDir);
    const float uniformPhaseValue = 1.0f / (4.0f * PI);
    const float miePhaseValue = computeMiePhaseFunction(atmosphere.miePhaseG, -cosTheta);
    const float rayleighPhaseValue = computeRayleighPhaseFunction(cosTheta);

    vec3 globalL = atmosphere.sunIlluminance.rgb;

    // Ray march the atmosphere to integrate optical depth
    vec3 L = vec3(0.0f);
    vec3 throughput = vec3(1.0);
    vec3 OpticalDepth = vec3(0.0);
    float t = 0.0f;
    const float SampleSegmentT = 0.3f;
    for (float s = 0.0f; s < SampleCount; s += 1.0f)
    {
        //t = tMax * (s + SampleSegmentT) / SampleCount;
        // Exact difference, important for accuracy of multiple scattering
        float NewT = tMax * (s + SampleSegmentT) / SampleCount;
        dt = NewT - t;
        t = NewT;
        vec3 P = WorldPos + t * WorldDir;

        MediumSample medium = sampleMedium(P, atmosphere);
        const vec3 SampleOpticalDepth = medium.extinction * dt;
        const vec3 SampleTransmittance = exp(-SampleOpticalDepth);
        OpticalDepth += SampleOpticalDepth;

        const float pHeight = length(P);
        const vec3 UpVector = P / pHeight;
        const float sunZenithCosAngle = dot(SunDir, UpVector);
        const vec3 transmittanceToSun = sampleTransmittanceLut(atmosphere.bottomRadius, atmosphere.topRadius,
            pHeight, sunZenithCosAngle);

        const vec3 phaseTimesScattering = medium.scatteringMie * miePhaseValue + medium.scatteringRay * rayleighPhaseValue;

        // Earth shadow 
        const float tEarth = raySphereIntersectNearest(P, SunDir, earthO + PlanetRadiusOffset * UpVector, atmosphere.bottomRadius);
        const float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

        const float shadow = getShadow(atmosphere, P);;

        const vec3 S = globalL * (earthShadow * shadow * transmittanceToSun * phaseTimesScattering * medium.scattering);


        const vec3 MS = medium.scattering;
        const vec3 integralMS = (MS - MS * SampleTransmittance) / medium.extinction;
        result.MultiScatAs1 += throughput * integralMS;

        const vec3 integralS = (S - S * SampleTransmittance) / medium.extinction;
        L += throughput * integralS;
        throughput *= SampleTransmittance;
    }

    if (ground && tMax == tBottom && tBottom > 0.0)
    {
        // Account for bounced light off the earth
        vec3 P = WorldPos + tBottom * WorldDir;

        const float pHeight = length(P);
        const vec3 UpVector = P / pHeight;
        const float sunZenithCosAngle = dot(SunDir, UpVector);
        const vec3 transmittanceToSun = sampleTransmittanceLut(atmosphere.bottomRadius, atmosphere.topRadius,
            pHeight, sunZenithCosAngle);

        const float NdotL = clamp(dot(normalize(UpVector), normalize(SunDir)), 0, 1);
        L += globalL * transmittanceToSun * throughput * NdotL * atmosphere.groundAlbedo / PI;
    }

    result.L = L;
    return result;
}

shared vec3 multiScatShared[64];
shared vec3 radianceShared[64];


void main()
{
    const vec2 pixPos = vec2(gl_GlobalInvocationID.xy) + 0.5f;
    const float lutSize = atmosphere.multiScatteringLutResolution;
    vec2 uv = pixPos / lutSize;
    uv = vec2(fromSubUvsToUnit(uv.x, lutSize), fromSubUvsToUnit(uv.y, lutSize));

    const vec2 ndcPos = pixPos / vec2(lutSize) * vec2(2.0f, -2.0f) - vec2(1.0f, -1.0f);

    const float cosSunZenithAngle = uv.x * 2.0 - 1.0;
    const float sinSunZenithAngle = sqrt(clamp(1.0 - cosSunZenithAngle * cosSunZenithAngle, 0, 1));
    const vec3 sunDir = vec3(0.0, cosSunZenithAngle, -sinSunZenithAngle);
    // We adjust again viewHeight according to PLANET_RADIUS_OFFSET to be in a valid range.
    const float viewHeight = atmosphere.bottomRadius + clamp(uv.y + PlanetRadiusOffset, 0, 1) * (atmosphere.topRadius - atmosphere.bottomRadius - PlanetRadiusOffset);

    const vec3 worldPos = vec3(0.0f, viewHeight, 0.0f);


    const bool ground = true;
    const float sampleCount = 20;
    const float DepthBufferValue = -1.0;
    const bool MieRayPhase = false;

    const float sphereSolidAngle = 4.0f * PI;
    const float isotropicPhaseValue = 1.0f / sphereSolidAngle;


    // Reference. Since there are many sample, it requires MULTI_SCATTERING_POWER_SERIE to be true for accuracy and to avoid divergences (see declaration for explanations)
    const int SQRTSAMPLECOUNT = 8;
    const float sqrtSample = float(SQRTSAMPLECOUNT);
    float i = 0.5f + float(gl_GlobalInvocationID.z / SQRTSAMPLECOUNT);
    float j = 0.5f + float(gl_GlobalInvocationID.z - float((gl_GlobalInvocationID.z / SQRTSAMPLECOUNT)*SQRTSAMPLECOUNT));
    {
        const float randA = i / sqrtSample;
        const float randB = j / sqrtSample;
        const float theta = 2.0f * PI * randA;
        const float phi = PI * randB;
        const float cosPhi = cos(phi);
        const float sinPhi = sin(phi);
        const float cosTheta = cos(theta);
        const float sinTheta = sin(theta);
        const vec3 worldDir = vec3(cosTheta * sinPhi, cosPhi, -sinTheta * sinPhi);
        SingleScatteringResult result = IntegrateScatteredLuminance(
            ndcPos,
            worldPos,
            worldDir,
            sunDir,
            atmosphere,
            ground,
            sampleCount,
            DepthBufferValue,
            MieRayPhase,
            9000000.0f);

        const float weight = sphereSolidAngle / (sqrtSample * sqrtSample);
        multiScatShared[gl_GlobalInvocationID.z] = result.MultiScatAs1 * weight;
        radianceShared[gl_GlobalInvocationID.z] = result.L * weight;
    }

    groupMemoryBarrier();
    barrier();

    for (uint i = 32; i >= 1; i /= 2)
    {
        if (gl_GlobalInvocationID.z < i)
        {
            multiScatShared[gl_GlobalInvocationID.z] += multiScatShared[gl_GlobalInvocationID.z + i];
            radianceShared[gl_GlobalInvocationID.z] += radianceShared[gl_GlobalInvocationID.z + i];
        }
        groupMemoryBarrier();
        barrier();
    }

    if (gl_GlobalInvocationID.z > 0)
        return;

    vec3 multiScatteredRadiance = multiScatShared[0] * isotropicPhaseValue;	// Equation 7 f_ms
    vec3 radiance	= radianceShared[0] * isotropicPhaseValue;				// Equation 5 L_2ndOrder

    // MultiScatAs1 represents the amount of luminance scattered as if the integral of scattered luminance over the sphere would be 1.
    //  - 1st order of scattering: one can ray-march a straight path as usual over the sphere. That is InScatteredLuminance.
    //  - 2nd order of scattering: the inscattered luminance is InScatteredLuminance at each of samples of fist order integration. Assuming a uniform phase function that is represented by MultiScatAs1,
    //  - 3nd order of scattering: the inscattered luminance is (InScatteredLuminance * MultiScatAs1 * MultiScatAs1)
    //  - etc.
    // For a serie, sum_{n=0}^{n=+inf} = 1 + r + r^2 + r^3 + ... + r^n = 1 / (1.0 - r), see https://en.wikipedia.org/wiki/Geometric_series 
    const vec3 multiScatteringSum = 1.0f / (1.0 - multiScatteredRadiance);
    const vec3 L = radiance * multiScatteringSum;// Equation 10 Psi_ms
    imageStore(multiScatteringLut, ivec2(gl_GlobalInvocationID.xy), vec4(L, 1.0f));
}
