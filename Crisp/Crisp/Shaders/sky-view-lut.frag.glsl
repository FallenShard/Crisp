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

float fromUnitToSubUvs(float u, float resolution) { return (u + 0.5f / resolution) * (resolution / (resolution + 1.0f)); }
float fromSubUvsToUnit(float u, float resolution) { return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f)); }

void uvToSkyViewLutParams(in vec2 uv, in float bottomRadius, in float viewHeight, out float viewZenithCosAngle, out float lightViewCosAngle)
{
    uv = vec2(fromSubUvsToUnit(uv.x, 192.0f), fromSubUvsToUnit(uv.y, 108.0f));

    const float horizonDist = sqrt(viewHeight * viewHeight - bottomRadius * bottomRadius);
    const float horizonAngle = acos(horizonDist / viewHeight);
    const float zenithHorizonAngle = PI - horizonAngle; // in [0, PI]

    if (uv.y < 0.5f)
    {
        float coord = 2.0f * uv.y; // in [0, 1] because we enter here if below horizon.
        coord = 1.0f - coord; // in [1, 0]
        coord *= coord; // quadratic spread for [1, 0]
        coord = 1.0f - coord; // in [0, 1]
        viewZenithCosAngle = cos(zenithHorizonAngle * coord);
    }
    else
    {
        float coord = 2.0f * uv.y - 1.0f; // in [0, 1]
        coord *= coord;
        viewZenithCosAngle = cos(zenithHorizonAngle + horizonAngle * coord);
    }

    const float coord = uv.x * uv.x;
    lightViewCosAngle = -(coord * 2.0f - 1.0f);
}

vec3 sampleTransmittanceLut(const AtmosphereParams atmosphere, const float viewHeight, const float viewZenithCosAngle)
{
    const float horizon = sqrt(max(0.0f, atmosphere.topRadius * atmosphere.topRadius - atmosphere.bottomRadius * atmosphere.bottomRadius));
    const float rho = sqrt(max(0.0f, viewHeight * viewHeight - atmosphere.bottomRadius * atmosphere.bottomRadius));

    const float discriminant = viewHeight * viewHeight * (viewZenithCosAngle * viewZenithCosAngle - 1.0) + atmosphere.topRadius * atmosphere.topRadius;
    const float d = max(0.0, (-viewHeight * viewZenithCosAngle + sqrt(discriminant))); // Distance to atmosphere boundary

    const float d_min = atmosphere.topRadius - viewHeight;
    const float d_max = rho + horizon;
    const float x_mu = (d - d_min) / (d_max - d_min);
    const float x_r = rho / horizon;

    return textureLod(transmittanceLut, vec2(x_mu, x_r), 0).rgb;
}

vec3 sampleMultipleScattering(const AtmosphereParams atmosphere, const float viewHeight, float viewZenithCosAngle)
{
    vec2 uv = clamp(vec2(viewZenithCosAngle * 0.5f + 0.5f, (viewHeight - atmosphere.bottomRadius) / (atmosphere.topRadius - atmosphere.bottomRadius)), vec2(0), vec2(1));
    uv = vec2(fromUnitToSubUvs(uv.x, atmosphere.multiScatteringLutResolution), fromUnitToSubUvs(uv.y, atmosphere.multiScatteringLutResolution));

    return textureLod(multiScatteringLut, uv, 0).rgb;
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

vec3 integrateScatteredLuminance(const vec3 worldPos, const vec3 worldDir, const vec3 sunDir,
    in AtmosphereParams atmosphere, const float SampleCountIni, const bool VariableSampleCount)
{
    // Compute next intersection with atmosphere or ground 
    const vec3 earthCenter = vec3(0.0f, 0.0f, 0.0f);
    float tMax = intersectAtmosphere(worldPos, worldDir, atmosphere.bottomRadius, atmosphere.topRadius);
    if (tMax < 0.0f)
        return vec3(0.0f);

    tMax = min(tMax, 9000000.0f);

    // Sample count 
    float SampleCount = SampleCountIni;
    float SampleCountFloor = SampleCountIni;
    float tMaxFloor = tMax;
    if (VariableSampleCount)
    {
        SampleCount = mix(atmosphere.minRayMarchingSamples, atmosphere.maxRayMarchingSamples, clamp(tMax * 0.01f, 0.0f, 1.0f));
        SampleCountFloor = floor(SampleCount);
        tMaxFloor = tMax * SampleCountFloor / SampleCount;	// rescale tMax to map to the last entire step segment.
    }
    float dt = tMax / SampleCount;
    

    // Phase functions
    const float uniformPhase = 1.0 / (4.0 * PI);
    const vec3 wi = sunDir;
    const vec3 wo = worldDir;
    const float cosTheta = dot(wi, wo);

    // negate cosTheta because due to WorldDir being a "in" direction. 
    const float miePhaseValue = computeMiePhaseFunction(atmosphere.miePhaseG, -cosTheta);
    const float rayleighPhaseValue = computeRayleighPhaseFunction(cosTheta);
    const vec3 globalL = atmosphere.sunIlluminance.rgb;

    // Ray march the atmosphere to integrate luminance.
    vec3 L = vec3(0.0f);
    vec3 throughput = vec3(1.0f);
    float t = 0.0f;
    const float segmentBias = 0.3f;
    for (float s = 0.0f; s < SampleCount; s += 1.0f)
    {
        if (VariableSampleCount)
        {
            // More expensive, but artifact-free.
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
            }
            else
            {
                t1 = tMaxFloor * t1;
            }
            dt = t1 - t0;
            t = t0 + (t1 - t0) * segmentBias;
        }
        else
        {
            const float tNext = tMax * (s + segmentBias) / SampleCount;
            dt = tNext - t;
            t = tNext;
        }

        // Sample the transmittance for the current segment part.
        const vec3 pos = worldPos + t * worldDir;
        const MediumSample medium = sampleMedium(pos, atmosphere);
        const vec3 stepTransmittance = exp(-medium.extinction * dt);

        const float posHeight = length(pos);
        const vec3 upVec = pos / posHeight;
        const float sunZenithCosAngle = dot(sunDir, upVec);
        const vec3 transmittanceToSun = sampleTransmittanceLut(atmosphere, posHeight, sunZenithCosAngle);

        const vec3 phaseTimesScattering =  medium.scatteringMie * miePhaseValue + medium.scatteringRay * rayleighPhaseValue;

        // Earth shadow 
        const float tEarth = raySphereIntersectNearest(pos, sunDir, earthCenter + PlanetRadiusOffset * upVec, atmosphere.bottomRadius);
        const float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;
        
        // First evaluate opaque shadow
        const float shadow = getShadow(atmosphere, pos);

        const vec3 multiScatteredL = sampleMultipleScattering(atmosphere, posHeight, sunZenithCosAngle);
        const vec3 S = globalL * (earthShadow * shadow * transmittanceToSun * phaseTimesScattering + multiScatteredL * medium.scattering);

        // See slide 28 at http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/.
        // Integrate along the current step segment.
        const vec3 integralS = (S - S * stepTransmittance) / medium.extinction;

        // Accumulate and also take into account the transmittance from previous steps.
        L += throughput * integralS;
        throughput *= stepTransmittance;
    }

    return L;
}

void main()
{
    const vec2 pixPos = vec2(gl_FragCoord.xy);
    const vec2 uv = pixPos / vec2(192.0f, 108.0f);

    const vec3 clipPos = vec3(uv * vec2(2.0, -2.0) - vec2(1.0, -1.0), 1.0);
    const vec4 homogPos = atmosphere.invVP * vec4(clipPos, 1.0);
    const vec3 eyePos = homogPos.xyz / homogPos.w;

    vec3 worldDir = normalize(eyePos - atmosphere.cameraPosition);
    vec3 worldPos = atmosphere.cameraPosition + vec3(0, atmosphere.bottomRadius, 0);
    const float viewHeight = length(worldPos);

    float viewZenithCosAngle;
    float lightViewCosAngle;
    uvToSkyViewLutParams(uv, atmosphere.bottomRadius, viewHeight, viewZenithCosAngle, lightViewCosAngle);

    vec3 SunDir;
    {
      const vec3 upVec = worldPos / viewHeight;
      const float sunZenithCosAngle = dot(upVec, atmosphere.sunDirection);
      const float sunZenithSinAngle = sqrt(1.0 - sunZenithCosAngle * sunZenithCosAngle);
      SunDir = normalize(vec3(sunZenithSinAngle, sunZenithCosAngle, 0.0f));
    }

    worldPos = vec3(0.0f, viewHeight, 0.0f);

    const float viewZenithSinAngle = sqrt(max(0.0f, 1.0f - viewZenithCosAngle * viewZenithCosAngle));
    worldDir = vec3(
      viewZenithSinAngle * lightViewCosAngle,
      viewZenithCosAngle,
      -viewZenithSinAngle * sqrt(max(0, 1.0 - lightViewCosAngle * lightViewCosAngle))
      );

    // Move to top atmospehre
    if (!moveToTopAtmosphere(worldPos, worldDir, atmosphere.topRadius))
    {
      // Ray is not intersecting the atmosphere
      finalColor = vec4(0, 0, 0, 1);
      return;
    }

    const float SampleCountIni = 30;
    const bool VariableSampleCount = true;
    const vec3 L = integrateScatteredLuminance(worldPos, worldDir, SunDir, atmosphere, SampleCountIni, VariableSampleCount);
    finalColor = vec4(L, 1.0f);
}