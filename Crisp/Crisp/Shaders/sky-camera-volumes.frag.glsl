#version 450 core


#define PI 3.1415926535897932384626433832795
#define AP_SLICE_COUNT 32.0f
#define AP_KM_PER_SLICE 4.0f
#define PLANET_RADIUS_OFFSET 0.01f

layout(location = 0) out vec4 finalColor;

#include "Common/atmosphere.part.glsl"

layout(set = 0, binding = 0) uniform AtmosphereParamsBlock
{
    AtmosphereParams atmosphere;
};

layout(set = 1, binding = 0) uniform sampler2D transmittanceLut;
layout(set = 1, binding = 1) uniform sampler2D multiScatteringLut;

float CornetteShanksMiePhaseFunction(float g, float cosTheta)
{
    float k = 3.0 / (8.0 * PI) * (1.0 - g * g) / (2.0 + g * g);
    return k * (1.0 + cosTheta * cosTheta) / pow(1.0 + g * g - 2.0 * g * -cosTheta, 1.5);
}

float hgPhase(float g, float cosTheta)
{
//#ifdef USE_CornetteShanks
    //return CornetteShanksMiePhaseFunction(g, cosTheta);
// #else
// 	// Reference implementation (i.e. not schlick approximation). 
// 	// See http://www.pbr-book.org/3ed-2018/Volume_Scattering/Phase_Functions.html
    float numer = 1.0f - g * g;
    float denom = 1.0f + g * g + 2.0f * g * cosTheta;
    return numer / (4.0f * PI * denom * sqrt(denom));
// #endif
}

float RayleighPhase(float cosTheta)
{
    float factor = 3.0f / (16.0f * PI);
    return factor * (1.0f + cosTheta * cosTheta);
}


void LutTransmittanceParamsToUv(in AtmosphereParams atmosphere, in float viewHeight, in float viewZenithCosAngle, out vec2 uv)
{
    float H = sqrt(max(0.0f, atmosphere.topRadius * atmosphere.topRadius - atmosphere.bottomRadius * atmosphere.bottomRadius));
    float rho = sqrt(max(0.0f, viewHeight * viewHeight - atmosphere.bottomRadius * atmosphere.bottomRadius));

    float discriminant = viewHeight * viewHeight * (viewZenithCosAngle * viewZenithCosAngle - 1.0) + atmosphere.topRadius * atmosphere.topRadius;
    float d = max(0.0, (-viewHeight * viewZenithCosAngle + sqrt(discriminant))); // Distance to atmosphere boundary

    float d_min = atmosphere.topRadius - viewHeight;
    float d_max = rho + H;
    float x_mu = (d - d_min) / (d_max - d_min);
    float x_r = rho / H;

    uv = vec2(x_mu, x_r);
    //uv = float2(fromUnitToSubUvs(uv.x, TRANSMITTANCE_TEXTURE_WIDTH), fromUnitToSubUvs(uv.y, TRANSMITTANCE_TEXTURE_HEIGHT)); // No real impact so off
}



float fromUnitToSubUvs(float u, float resolution) { return (u + 0.5f / resolution) * (resolution / (resolution + 1.0f)); }
float fromSubUvsToUnit(float u, float resolution) { return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f)); }

// vec3 GetMultipleScattering(AtmosphereParameters Atmosphere, vec3 scattering, vec3 extinction, vec3 worlPos, float viewZenithCosAngle)
// {
// 	vec2 uv = clamp(vec2(viewZenithCosAngle*0.5f + 0.5f, (length(worlPos) - Atmosphere.BottomRadius) / (Atmosphere.TopRadius - Atmosphere.BottomRadius)), vec2(0), vec2(1));
// 	uv = vec2(fromUnitToSubUvs(uv.x, MultiScatteringLUTRes), fromUnitToSubUvs(uv.y, MultiScatteringLUTRes));

// 	vec3 multiScatteredLuminance = MultiScatTexture.SampleLevel(samplerLinearClamp, uv, 0).rgb;
// 	return multiScatteredLuminance;
// }

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

struct SingleScatteringResult
{
    vec3 L;						// Scattered light (luminance)
    vec3 OpticalDepth;			// Optical depth (1/m)
    vec3 Transmittance;			// Transmittance in [0,1] (unitless)
    vec3 MultiScatAs1;

    vec3 NewMultiScatStep0Out;
    vec3 NewMultiScatStep1Out;

    vec3 Debug;
};

vec3 sampleMultipleScattering(const AtmosphereParams atmosphere, const float viewHeight, float viewZenithCosAngle)
{
    vec2 uv = clamp(vec2(viewZenithCosAngle * 0.5f + 0.5f, (viewHeight - atmosphere.bottomRadius) / (atmosphere.topRadius - atmosphere.bottomRadius)), vec2(0), vec2(1));
    uv = vec2(fromUnitToSubUvs(uv.x, atmosphere.multiScatteringLutResolution), fromUnitToSubUvs(uv.y, atmosphere.multiScatteringLutResolution));

    return textureLod(multiScatteringLut, uv, 0).rgb;
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
    in vec2 pixPos, in vec3 WorldPos, in vec3 WorldDir, in vec3 SunDir, const in AtmosphereParams atmosphere,
    in bool ground, in float SampleCountIni, in float DepthBufferValue,
    in bool MieRayPhase, float tMaxMax)
{
    SingleScatteringResult result;
    result.L = vec3(0.0f);
    result.OpticalDepth = vec3(0.0f);			// Optical depth (1/m)
    result.Transmittance = vec3(0.0f);			// Transmittance in [0,1] (unitless)
    result.MultiScatAs1 = vec3(0.0f);
    result.NewMultiScatStep0Out = vec3(0.0f);
    result.NewMultiScatStep1Out = vec3(0.0f);

    vec3 ClipSpace = vec3((pixPos / vec2(32, 32)) * vec2(2.0, -2.0) - vec2(1.0, -1.0), 1.0);

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
    float dt = tMax / SampleCount;
    

    // Phase functions
    const float uniformPhase = 1.0 / (4.0 * PI);
    const vec3 wi = SunDir;
    const vec3 wo = WorldDir;
    float cosTheta = dot(wi, wo);
    const float miePhaseValue = computeMiePhaseFunction(atmosphere.miePhaseG, -cosTheta);	// mnegate cosTheta because due to WorldDir being a "in" direction. 
    const float rayleighPhaseValue = computeRayleighPhaseFunction(cosTheta);// mnegate cosTheta because due to WorldDir being a "in" direction. 
    const vec3 globalL = atmosphere.sunIlluminance.rgb;

    // Ray march the atmosphere to integrate optical depth
    vec3 L = vec3(0.0f);
    vec3 throughput = vec3(1.0);
    vec3 OpticalDepth = vec3(0.0);
    float t = 0.0f;
    float tPrev = 0.0;
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

        float pHeight = length(P);
        const vec3 UpVector = P / pHeight;
        float SunZenithCosAngle = dot(SunDir, UpVector);
        const vec3 transmittanceToSun = sampleTransmittanceLut(atmosphere.bottomRadius, atmosphere.topRadius,
            pHeight, SunZenithCosAngle);

        const vec3 phaseTimesScattering = medium.scatteringMie * miePhaseValue + medium.scatteringRay * rayleighPhaseValue;


        // Earth shadow 
        const float tEarth = raySphereIntersectNearest(P, SunDir, earthO + PlanetRadiusOffset * UpVector, atmosphere.bottomRadius);
        const float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

        const vec3 multiScatteredL = sampleMultipleScattering(atmosphere, pHeight, SunZenithCosAngle);


        const float shadow = getShadow(atmosphere, P);

        const vec3 S = globalL * (earthShadow * shadow * transmittanceToSun * phaseTimesScattering + multiScatteredL * medium.scattering);
        const vec3 integralS = (S - S * SampleTransmittance) / medium.extinction;
        L += throughput * integralS;
        throughput *= SampleTransmittance;

        tPrev = t;
    }

//    if (ground && tMax == tBottom && tBottom > 0.0)
//    {
//        // Account for bounced light off the earth
//        vec3 P = WorldPos + tBottom * WorldDir;
//        float pHeight = length(P);
//
//        const vec3 UpVector = P / pHeight;
//        float SunZenithCosAngle = dot(SunDir, UpVector);
//        vec2 uv;
//        LutTransmittanceParamsToUv(Atmosphere, pHeight, SunZenithCosAngle, uv);
//        vec3 TransmittanceToSun = textureLod(TransmittanceLutTexture, uv, 0).rgb;
//
//        const float NdotL = clamp(dot(normalize(UpVector), normalize(SunDir)), 0, 1);
//        L += globalL * TransmittanceToSun * throughput * NdotL * Atmosphere.GroundAlbedo / PI;
//    }

    result.L = L;
    result.OpticalDepth = OpticalDepth;
    result.Transmittance = throughput;
    return result;
}

void main()
{
    // Compute viewing direction for the given pixel
    const vec2 pixPos = gl_FragCoord.xy;
    const vec3 ndcPos = vec3((pixPos / vec2(32, 32)) * vec2(2.0, -2.0) - vec2(1.0, -1.0), 0.5);
    const vec4 HPos = atmosphere.invVP * vec4(ndcPos, 1.0);
    const vec3 eyePos = HPos.xyz / HPos.w;
    vec3 worldDir = normalize(eyePos - atmosphere.cameraPosition);

    // In ECEF coordinates
    vec3 worldPosEcef = atmosphere.cameraPosition + vec3(0.0f, atmosphere.bottomRadius, 0.0f);
    vec3 worldPos = worldPosEcef;
    

    float sliceT = (float(gl_Layer) + 0.5f) / AP_SLICE_COUNT;
    sliceT *= sliceT;	// squared distribution
    sliceT *= AP_SLICE_COUNT; // in [0, 1]

    // Compute position from froxel information
    float tMax = sliceT * AP_KM_PER_SLICE;
    vec3 newWorldPos = worldPos + tMax * worldDir;

    // If the voxel is under the ground, make sure to offset it out on the ground.
    float viewHeight = length(newWorldPos);
    
    if (viewHeight <= (atmosphere.bottomRadius + PLANET_RADIUS_OFFSET))
    {
        // Apply a position offset to make sure no artefact are visible close to the earth boundaries for large voxel.
        newWorldPos = normalize(newWorldPos) * (atmosphere.bottomRadius + PLANET_RADIUS_OFFSET + 0.001f);
        worldDir = normalize(newWorldPos - worldPosEcef);
        tMax = length(newWorldPos - worldPosEcef);
    }
    float tMaxMax = tMax;
    
    // Move ray marching start up to top atmosphere.
    viewHeight = length(worldPos);
    if (viewHeight >= atmosphere.topRadius)
    {
        vec3 prevWorldPos = worldPos;
        if (!moveToTopAtmosphere(worldPos, worldDir, atmosphere.topRadius))
        {
            // Ray is not intersecting the atmosphere
            finalColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
            return;
        }
        const float lengthToAtmosphere = length(prevWorldPos - worldPos);
        if (tMaxMax < lengthToAtmosphere)
        {
            // tMaxMax for this voxel is not within earth atmosphere
            finalColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
            return;
        }
        // Now world position has been moved to the atmosphere boundary: we need to reduce tMaxMax accordingly. 
        tMaxMax = max(0.0, tMaxMax - lengthToAtmosphere);
    }

    


    const bool ground = false;
    const float SampleCountIni = max(1.0, float(gl_Layer + 1.0) * 2.0f);
    const float DepthBufferValue = -1.0;
    const bool MieRayPhase = true;
    const vec3 sunDir = atmosphere.sunDirection;
    const vec3 sunLuminance = vec3(0.0);
    SingleScatteringResult ss = IntegrateScatteredLuminance(pixPos, worldPos, worldDir, sunDir, atmosphere, ground, SampleCountIni, DepthBufferValue, MieRayPhase, tMaxMax);

    const float Transmittance = dot(ss.Transmittance, vec3(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f));
    finalColor = vec4(ss.L, 1.0 - Transmittance);
}