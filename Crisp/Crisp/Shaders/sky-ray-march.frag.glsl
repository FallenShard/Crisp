#version 450 core
#define PI 3.14159265358979323846

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 finalColor;

#include "Parts/atmosphere.part.glsl"

layout(set = 0, binding = 0) uniform AtmosphereParamsBlock
{
    AtmosphereParams atmosphere;
};

layout(set = 1, binding = 0) uniform sampler2D transmittanceLut;
layout(set = 1, binding = 1) uniform sampler2D multiScatteringLut;
layout(set = 1, binding = 2) uniform sampler2D SkyViewLutTexture;
layout(set = 1, binding = 3) uniform sampler2DArray AtmosphereCameraScatteringVolume;
layout(set = 1, binding = 4) uniform sampler2D ViewDepthTexture;


struct SingleScatteringResult
{
    vec3 L;						// Scattered light (luminance)
    vec3 Transmittance;			// Transmittance in [0,1] (unitless)
};

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

float getShadow(in AtmosphereParams atmosphere, vec3 P)
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

float fromUnitToSubUvs(float u, float resolution) { return (u + 0.5f / resolution) * (resolution / (resolution + 1.0f)); }

vec3 sampleMultipleScattering(const AtmosphereParams atmosphere, const float viewHeight, float viewZenithCosAngle)
{
    vec2 uv = clamp(vec2(viewZenithCosAngle * 0.5f + 0.5f, (viewHeight - atmosphere.bottomRadius) / (atmosphere.topRadius - atmosphere.bottomRadius)), vec2(0), vec2(1));
    uv = vec2(fromUnitToSubUvs(uv.x, atmosphere.multiScatteringLutResolution), fromUnitToSubUvs(uv.y, atmosphere.multiScatteringLutResolution));

    return textureLod(multiScatteringLut, uv, 0).rgb;
}

SingleScatteringResult integrateScatteredLuminance(
    in vec2 pixPos, in vec3 WorldPos, in vec3 WorldDir, in vec3 SunDir, in AtmosphereParams atmosphere,
    in bool ground, in float SampleCountIni, in float DepthBufferValue, in bool VariableSampleCount, float tMaxMax)
{
    tMaxMax = 9000000.0f;
    SingleScatteringResult result;
    result.L = vec3(0.0f);
    result.Transmittance = vec3(0.0f);			// Transmittance in [0,1] (unitless)

    vec3 ClipSpace = vec3((pixPos / vec2(atmosphere.screenResolution)) * vec2(2.0f, -2.0f) - vec2(1.0f, -1.0f), 1.0f);

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

    if (DepthBufferValue >= 0.0f)
    {
        ClipSpace.z = DepthBufferValue;
        if (ClipSpace.z < 1.0f)
        {
            vec4 DepthBufferWorldPos = atmosphere.invVP * vec4(ClipSpace, 1.0);
            DepthBufferWorldPos /= DepthBufferWorldPos.w;

            float tDepth = length(DepthBufferWorldPos.xyz - (WorldPos + vec3(0.0, 0.0, -atmosphere.bottomRadius))); // apply earth offset to go back to origin as top of earth mode. 
            if (tDepth < tMax)
            {
                tMax = tDepth;
            }
        }
        //		if (VariableSampleCount && ClipSpace.z == 1.0f)
        //			return result;
    }
    tMax = min(tMax, tMaxMax);

    // Sample count 
    float SampleCount = SampleCountIni;
    float SampleCountFloor = SampleCountIni;
    float tMaxFloor = tMax;
    if (VariableSampleCount)
    {
        SampleCount = mix(atmosphere.minRayMarchingSamples, atmosphere.maxRayMarchingSamples, clamp(tMax*0.01, 0, 1));
        SampleCountFloor = floor(SampleCount);
        tMaxFloor = tMax * SampleCountFloor / SampleCount;	// rescale tMax to map to the last entire step segment.
    }
    float dt = tMax / SampleCount;

    // Phase functions
    const float cosTheta = dot(SunDir, WorldDir);
    const float miePhaseValue = computeMiePhaseFunction(atmosphere.miePhaseG, -cosTheta);	// mnegate cosTheta because due to WorldDir being a "in" direction. 
    const float rayleighPhaseValue = computeRayleighPhaseFunction(cosTheta);
    const vec3 globalL = atmosphere.sunIlluminance.rgb;

    // Ray march the atmosphere to integrate optical depth
    vec3 L = vec3(0.0f);
    vec3 throughput = vec3(1.0);
    float t = 0.0f;
    const float SampleSegmentT = 0.3f;
    for (float s = 0.0f; s < SampleCount; s += 1.0f)
    {
        if (VariableSampleCount)
        {
            // More expenssive but artefact free
            float t0 = (s) / SampleCountFloor;
            float t1 = (s + 1.0f) / SampleCountFloor;
            // Non linear distribution of sample within the range.
            t0 = t0 * t0;
            t1 = t1 * t1;
            // Make t0 and t1 world space distances.
            t0 = tMaxFloor * t0;
            if (t1 > 1.0)
            {
                t1 = tMax;
                //	t1 = tMaxFloor;	// this reveal depth slices
            }
            else
            {
                t1 = tMaxFloor * t1;
            }
            t = t0 + (t1 - t0)*SampleSegmentT;
            dt = t1 - t0;
        }
        else
        {
            //t = tMax * (s + SampleSegmentT) / SampleCount;
            // Exact difference, important for accuracy of multiple scattering
            float NewT = tMax * (s + SampleSegmentT) / SampleCount;
            dt = NewT - t;
            t = NewT;
        }
        const vec3 P = WorldPos + t * WorldDir;

        MediumSample medium = sampleMedium(P, atmosphere);
        const vec3 stepTransmittance = exp(-medium.extinction * dt);

        const float posHeight = length(P);
        const vec3 UpVector = P / posHeight;
        const float sunZenithCosAngle = dot(SunDir, UpVector);
        const vec3 transmittanceToSun = sampleTransmittanceLut(atmosphere.bottomRadius, atmosphere.topRadius,
            posHeight, sunZenithCosAngle);

        const vec3 phaseTimesScattering = medium.scatteringMie * miePhaseValue + medium.scatteringRay * rayleighPhaseValue;

        // Earth shadow 
        const float tEarth = raySphereIntersectNearest(P, SunDir, earthO + PlanetRadiusOffset * UpVector, atmosphere.bottomRadius);
        const float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

        const vec3 multiScatteredL = sampleMultipleScattering(atmosphere, posHeight, sunZenithCosAngle);

        const float shadow = getShadow(atmosphere, P);

        const vec3 S = globalL * (earthShadow * shadow * transmittanceToSun * phaseTimesScattering + multiScatteredL * medium.scattering);
        const vec3 integralS = (S - S * stepTransmittance) / medium.extinction;
        L += throughput * integralS;
        throughput *= stepTransmittance;
    }

//    if (ground && tMax == tBottom && tBottom > 0.0)
//    {
//        // Account for bounced light off the earth
//        vec3 P = WorldPos + tBottom * WorldDir;
//        float pHeight = length(P);
//
//        const vec3 UpVector = P / pHeight;
//        float SunZenithCosAngle = dot(SunDir, UpVector);
//        const vec3 transmittanceToSun = sampleTransmittanceLut(Atmosphere.BottomRadius, Atmosphere.TopRadius,
//            pHeight, SunZenithCosAngle);
//
//        const float NdotL = clamp(dot(normalize(UpVector), normalize(SunDir)), 0, 1);
//        L += globalL * TransmittanceToSun * throughput * NdotL * Atmosphere.GroundAlbedo / PI;
//    }

    result.L = L;
    result.Transmittance = throughput;
    return result;
}

#define AP_SLICE_COUNT 32.0f
#define AP_KM_PER_SLICE 4.0f

float aerialPerspectiveDepthToSlice(float depth)
{
    return depth * (1.0f / AP_KM_PER_SLICE);
}
float aerialPerspectiveSliceToDepth(float slice)
{
    return slice * AP_KM_PER_SLICE;
}

vec3 getSunLuminance(vec3 worldPos, vec3 worldDir, const vec3 sunDirection, float planetRadius)
{
    // Check if we are looking towards the sun.
    if (dot(worldDir, sunDirection) > cos(0.5f * 0.505f * PI / 180.0f))
    {
        // Are we maybe intersecting the planet? If not, we are looking into the sun.
        const float t = raySphereIntersectNearest(worldPos, worldDir, vec3(0.0f, 0.0f, 0.0f), planetRadius);
        if (t < 0.0f)
        {
            return vec3(1000000.0);
        }
    }
    return vec3(0.0f);
}

void main()
{
    const vec2 pixPos = vec2(gl_FragCoord.xy);

    const vec2 ndcPos = pixPos / vec2(atmosphere.screenResolution) * vec2(2.0f, -2.0f) - vec2(1.0f, -1.0f);
    vec4 homogPos = atmosphere.invVP * vec4(ndcPos, 1.0f, 1.0f);
    const vec3 targetWorldPos = homogPos.xyz / homogPos.w;
    vec3 worldDir = normalize(targetWorldPos - atmosphere.cameraPosition);

    vec3 worldPos = atmosphere.cameraPosition + vec3(0.0f, atmosphere.bottomRadius, 0.0f);
    const float viewHeight = length(worldPos);
    vec3 L = vec3(0.0f);

    float fragmentDepth = -1.0f;
    fragmentDepth = 1.0f;  // 1.0f - texture(ViewDepthTexture, pixPos).r;  // 1.0f;//ViewDepthTexture[pixPos].r;

    // If we have no obstacle in the view, just get the luminance from the sky.
    if (fragmentDepth == 1.0f)
        L += getSunLuminance(worldPos, worldDir, atmosphere.sunDirection, atmosphere.bottomRadius);

// #endif

//#if FASTAERIALPERSPECTIVE_ENABLED
//    homogPos = atmosphere.invVP * vec4(ndcPos, fragmentDepth, 1.0f);
//    const vec3 depthWorldPos = homogPos.xyz / homogPos.w;
//
//    // Figure out the depth slice that we need to sample our luminance from.
//    const float tDepth = length(depthWorldPos.xyz - (worldPos + vec3(0.0f, -atmosphere.bottomRadius, 0.0f)));
//    float slice = aerialPerspectiveDepthToSlice(tDepth);
//    float weight = 1.0;
//
//    // For slice 0, we weigh everything down to 0 at depth 0.
//    if (slice < 0.5)
//    {
//        weight = clamp(slice * 2.0, 0, 1);
//        slice = 0.5;
//    }
//    float w = sqrt(slice / AP_SLICE_COUNT);  // squared distribution

    //finalColor = vec4(tDepth, slice, weight, w);

//    const vec4 AP = weight * textureLod(AtmosphereCameraScatteringVolume, vec3(pixPos / vec2(atmosphere.screenResolution), w), 0);
//    L.rgb += AP.rgb;
//    float Opacity = AP.a;
//
//    finalColor = vec4(L, Opacity);


    //finalColor = vec4(L * 5, 1.0 - avgTransmittance);
    //utput.Luminance *= frac(clamp(w*AP_SLICE_COUNT, 0, AP_SLICE_COUNT));

//#else // FASTAERIALPERSPECTIVE_ENABLED

    // Move to top atmosphere as the starting point for ray marching.
    // This is critical to be after the above to not disrupt above atmosphere tests and voxel selection.
    if (!moveToTopAtmosphere(worldPos, worldDir, atmosphere.topRadius))
    {
        // Ray is not intersecting the atmosphere
        finalColor = vec4(getSunLuminance(worldPos, worldDir, atmosphere.sunDirection, atmosphere.bottomRadius), 1.0);
        return;
    }

    const bool ground = false;
    const float sampleCount = 0.0f;
    const bool variableSampleCount = true;
    const SingleScatteringResult ss = integrateScatteredLuminance(pixPos, worldPos, worldDir, atmosphere.sunDirection, atmosphere, ground, sampleCount, fragmentDepth, variableSampleCount, 9000000.0f);

    L += ss.L;
    const float avgTransmittance = dot(ss.Transmittance, vec3(1.0f / 3.0f));
    finalColor = vec4(L * 5, 1.0 - avgTransmittance);

// #endif // FASTAERIALPERSPECTIVE_ENABLED
}
