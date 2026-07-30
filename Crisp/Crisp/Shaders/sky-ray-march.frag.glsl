#version 450 core

#extension GL_GOOGLE_include_directive : require

#define PI 3.14159265358979323846

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 finalColor;

#include "Common/atmosphere.part.glsl"

layout(set = 0, binding = 0) uniform AtmosphereParamsBlock {
    AtmosphereParams atmosphere;
};

layout(set = 1, binding = 0) uniform sampler2D transmittanceLut;
layout(set = 1, binding = 1) uniform sampler2D multiScatteringLut;
layout(set = 1, binding = 2) uniform sampler2D SkyViewLutTexture;
layout(set = 1, binding = 3) uniform sampler2DArray AtmosphereCameraScatteringVolume;
layout(set = 1, binding = 4) uniform sampler2D ViewDepthTexture;

struct SingleScatteringResult {
    vec3 L;             // Scattered light (luminance)
    vec3 Transmittance; // Transmittance in [0,1] (unitless)
};

vec3 sampleTransmittanceLut(
    const float bottomRadius, const float topRadius, const float viewHeight, const float viewZenithCosAngle) {
    const float horizon = sqrt(max(0.0f, topRadius * topRadius - bottomRadius * bottomRadius));
    const float rho = sqrt(max(0.0f, viewHeight * viewHeight - bottomRadius * bottomRadius));

    const float discriminant =
        viewHeight * viewHeight * (viewZenithCosAngle * viewZenithCosAngle - 1.0) + topRadius * topRadius;
    const float d = max(0.0, (-viewHeight * viewZenithCosAngle + sqrt(discriminant))); // Distance to atmosphere
                                                                                       // boundary

    const float d_min = topRadius - viewHeight;
    const float d_max = rho + horizon;
    const float x_mu = (d - d_min) / (d_max - d_min);
    const float x_r = rho / horizon;

    return textureLod(transmittanceLut, vec2(x_mu, x_r), 0).rgb;
}

float getShadow(in AtmosphereParams atmosphere, vec3 P) {
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

vec3 sampleMultipleScattering(const AtmosphereParams atmosphere, const float viewHeight, float viewZenithCosAngle) {
    vec2 uv = clamp(
        vec2(
            viewZenithCosAngle * 0.5f + 0.5f,
            (viewHeight - atmosphere.bottomRadius) / (atmosphere.topRadius - atmosphere.bottomRadius)),
        vec2(0),
        vec2(1));
    uv = vec2(
        fromUnitToSubUvs(uv.x, atmosphere.multiScatteringLutResolution),
        fromUnitToSubUvs(uv.y, atmosphere.multiScatteringLutResolution));

    return textureLod(multiScatteringLut, uv, 0).rgb;
}

SingleScatteringResult integrateScatteredLuminance(
    in vec2 pixPos,
    in vec3 WorldPos,
    in vec3 WorldDir,
    in vec3 SunDir,
    in AtmosphereParams atmosphere,
    in bool ground,
    in float SampleCountIni,
    in float DepthBufferValue,
    in bool VariableSampleCount,
    float tMaxMax) {
    tMaxMax = 9000000.0f;
    SingleScatteringResult result;
    result.L = vec3(0.0f);
    result.Transmittance = vec3(0.0f); // Transmittance in [0,1] (unitless)

    vec3 ClipSpace = vec3(pixPos / vec2(atmosphere.screenResolution) * 2.0f - 1.0f, kFarPlaneDepth);

    // Compute next intersection with atmosphere or ground
    vec3 earthO = vec3(0.0f, 0.0f, 0.0f);
    float tBottom = raySphereIntersectNearest(WorldPos, WorldDir, earthO, atmosphere.bottomRadius);
    float tTop = raySphereIntersectNearest(WorldPos, WorldDir, earthO, atmosphere.topRadius);
    float tMax = 0.0f;
    if (tBottom < 0.0f) {
        if (tTop < 0.0f) {
            tMax = 0.0f; // No intersection with earth nor atmosphere: stop right away
            return result;
        } else {
            tMax = tTop;
        }
    } else {
        if (tTop > 0.0f) {
            tMax = min(tTop, tBottom);
        }
    }

    if (DepthBufferValue != kNoDepthBuffer) {
        ClipSpace.z = DepthBufferValue;
        if (ClipSpace.z > kFarPlaneDepth) {
            vec4 DepthBufferWorldPos = atmosphere.invVP * vec4(ClipSpace, 1.0);
            DepthBufferWorldPos /= DepthBufferWorldPos.w;

            // Undo the planet offset in WorldPos to match; this engine is Y up.
            float tDepth = length(DepthBufferWorldPos.xyz - (WorldPos + vec3(0.0, -atmosphere.bottomRadius, 0.0)));
            if (tDepth < tMax) {
                tMax = tDepth;
            }
        }
    }
    tMax = min(tMax, tMaxMax);

    // Sample count
    float SampleCount = SampleCountIni;
    float SampleCountFloor = SampleCountIni;
    float tMaxFloor = tMax;
    if (VariableSampleCount) {
        SampleCount = mix(atmosphere.minRayMarchingSamples, atmosphere.maxRayMarchingSamples, clamp(tMax * 0.01, 0, 1));
        SampleCountFloor = floor(SampleCount);
        tMaxFloor = tMax * SampleCountFloor / SampleCount; // rescale tMax to map to the last entire step segment.
    }
    float dt = tMax / SampleCount;

    // Phase functions
    const float cosTheta = dot(SunDir, WorldDir);
    const float miePhaseValue = computeMiePhaseFunction(atmosphere.miePhaseG, -cosTheta); // mnegate cosTheta because
                                                                                          // due to WorldDir being a
                                                                                          // "in" direction.
    const float rayleighPhaseValue = computeRayleighPhaseFunction(cosTheta);
    const vec3 globalL = atmosphere.sunIlluminance.rgb;

    // Ray march the atmosphere to integrate optical depth
    vec3 L = vec3(0.0f);
    vec3 throughput = vec3(1.0);
    float t = 0.0f;
    const float SampleSegmentT = 0.3f;
    for (float s = 0.0f; s < SampleCount; s += 1.0f) {
        if (VariableSampleCount) {
            // More expenssive but artefact free
            float t0 = (s) / SampleCountFloor;
            float t1 = (s + 1.0f) / SampleCountFloor;
            // Non linear distribution of sample within the range.
            t0 = t0 * t0;
            t1 = t1 * t1;
            // Make t0 and t1 world space distances.
            t0 = tMaxFloor * t0;
            if (t1 > 1.0) {
                t1 = tMax;
                //	t1 = tMaxFloor;	// this reveal depth slices
            } else {
                t1 = tMaxFloor * t1;
            }
            t = t0 + (t1 - t0) * SampleSegmentT;
            dt = t1 - t0;
        } else {
            // t = tMax * (s + SampleSegmentT) / SampleCount;
            //  Exact difference, important for accuracy of multiple scattering
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
        const vec3 transmittanceToSun =
            sampleTransmittanceLut(atmosphere.bottomRadius, atmosphere.topRadius, posHeight, sunZenithCosAngle);

        const vec3 phaseTimesScattering =
            medium.scatteringMie * miePhaseValue + medium.scatteringRay * rayleighPhaseValue;

        // Earth shadow
        const float tEarth =
            raySphereIntersectNearest(P, SunDir, earthO + PlanetRadiusOffset * UpVector, atmosphere.bottomRadius);
        const float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

        const vec3 multiScatteredL =
            atmosphere.multipleScatteringFactor * sampleMultipleScattering(atmosphere, posHeight, sunZenithCosAngle);

        const float shadow = getShadow(atmosphere, P);

        const vec3 S =
            globalL *
            (earthShadow * shadow * transmittanceToSun * phaseTimesScattering + multiScatteredL * medium.scattering);
        const vec3 integralS = (S - S * stepTransmittance) / medium.extinction;
        L += throughput * integralS;
        throughput *= stepTransmittance;
    }

    // The equality separates "ended on the planet" from "cut short by geometry", since the depth clamp above
    // can also have moved tMax.
    if (ground && tBottom > 0.0f && tMax == tBottom) {
        const vec3 P = WorldPos + tBottom * WorldDir;
        const float pHeight = length(P);
        const vec3 UpVector = P / pHeight;
        const float sunZenithCosAngle = dot(SunDir, UpVector);
        const vec3 transmittanceToSun =
            sampleTransmittanceLut(atmosphere.bottomRadius, atmosphere.topRadius, pHeight, sunZenithCosAngle);

        const float NdotL = clamp(sunZenithCosAngle, 0.0f, 1.0f);
        L += globalL * transmittanceToSun * throughput * NdotL * atmosphere.groundAlbedo / PI;
    }

    result.L = L;
    result.Transmittance = throughput;
    return result;
}

#define AP_SLICE_COUNT 32.0f
#define AP_KM_PER_SLICE 4.0f

float aerialPerspectiveDepthToSlice(float depth) {
    return depth * (1.0f / AP_KM_PER_SLICE);
}

float aerialPerspectiveSliceToDepth(float slice) {
    return slice * AP_KM_PER_SLICE;
}

// The solar disc dims towards its limb, faster at short wavelengths, which is what warms the rim.
//
// D. Hestroffer and C. Magnan, "Wavelength dependency of the Solar limb darkening", Astronomy and Astrophysics
// 333, 338-342 (1998). https://ui.adsabs.harvard.edu/abs/1998A&A...333..338H
// Equation 1 with u = 1 (their section 2) gives I(mu) = mu^alpha; centerToEdge is their r.
// Exponents from Table 2, Pierce and Slaughter solution, at 679.1 nm, 552.2 nm and 443.9 nm.
vec3 computeSunLimbDarkening(const float centerToEdge) {
    const vec3 alpha = vec3(0.397f, 0.503f, 0.652f);
    const float mu = sqrt(max(0.0f, 1.0f - centerToEdge * centerToEdge));
    return pow(vec3(mu), alpha);
}

// angleToSun and edgeFadeWidth come from the caller because the fade width needs a screen space derivative, which
// is only well defined in uniform control flow.
vec3 getSunLuminance(
    const AtmosphereParams atmosphere,
    const vec3 worldPos,
    const vec3 worldDir,
    const float angleToSun,
    const float edgeFadeWidth) {
    if (atmosphere.drawSunDisk == 0) {
        return vec3(0.0f);
    }

    // Fading rather than thresholding also dims a sub-pixel disc, which is right for what is really a coverage
    // fraction.
    const float sunAngularRadius = 0.5f * radians(atmosphere.sunAngularDiameterDegrees);
    const float coverage =
        1.0f - smoothstep(sunAngularRadius - edgeFadeWidth, sunAngularRadius + edgeFadeWidth, angleToSun);
    if (coverage <= 0.0f) {
        return vec3(0.0f);
    }

    if (raySphereIntersectNearest(worldPos, worldDir, vec3(0.0f), atmosphere.bottomRadius) >= 0.0f) {
        return vec3(0.0f);
    }

    const float centerToEdge = clamp(angleToSun / sunAngularRadius, 0.0f, 1.0f);
    return atmosphere.sunDiskLuminance * coverage * computeSunLimbDarkening(centerToEdge);
}

// Lets the LUT parameterizations be inspected directly instead of inferred from the final image.
bool tryRenderDebugView(const AtmosphereParams atmosphere, const vec2 uv, out vec4 color) {
    color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    switch (atmosphere.debugViewMode) {
    case 1:
        color.rgb = textureLod(transmittanceLut, uv, 0).rgb;
        return true;
    case 2:
        color.rgb = textureLod(multiScatteringLut, uv, 0).rgb * atmosphere.exposure;
        return true;
    case 3:
        color.rgb = textureLod(SkyViewLutTexture, uv, 0).rgb * atmosphere.exposure;
        return true;
    case 4:
        // Sweeps the froxel slices horizontally so the whole volume is visible at once.
        color.rgb =
            textureLod(
                AtmosphereCameraScatteringVolume,
                vec3(fract(uv * vec2(AP_SLICE_COUNT, 1.0f)), floor(uv.x * AP_SLICE_COUNT)),
                0)
                .rgb *
            atmosphere.exposure;
        return true;
    default:
        return false;
    }
}

void main() {
    const vec2 pixPos = vec2(gl_FragCoord.xy);
    const vec2 screenUv = pixPos / vec2(atmosphere.screenResolution);

    if (tryRenderDebugView(atmosphere, screenUv, finalColor)) {
        return;
    }

    const vec2 ndcPos = screenUv * 2.0f - 1.0f;
    vec4 homogPos = atmosphere.invVP * vec4(ndcPos, 1.0f, 1.0f);
    const vec3 targetWorldPos = homogPos.xyz / homogPos.w;
    vec3 worldDir = normalize(targetWorldPos - atmosphere.cameraPosition);

    vec3 worldPos = atmosphere.cameraPosition + vec3(0.0f, atmosphere.bottomRadius, 0.0f);
    const float viewHeight = length(worldPos);
    vec3 L = vec3(0.0f);

    // Must stay ahead of any branch that can diverge across a quad: fwidth is only defined in uniform control flow.
    const float angleToSun = acos(clamp(dot(worldDir, atmosphere.sunDirection), -1.0f, 1.0f));
    const float sunEdgeFadeWidth = max(fwidth(angleToSun), 1e-7f);

    // No depth pre-pass writes ViewDepthTexture yet, so every pixel is treated as open sky.
    const float fragmentDepth = kFarPlaneDepth; // textureLod(ViewDepthTexture, screenUv, 0).r;

    if (fragmentDepth == kFarPlaneDepth) {
        L += getSunLuminance(atmosphere, worldPos, worldDir, angleToSun, sunEdgeFadeWidth);
    }

    // The sky view LUT is only valid near the ground, since its vertical axis is built around the horizon as seen
    // from the camera. Above the fallback altitude we drop back to the full march.
    if (atmosphere.fastSkyEnabled != 0 && fragmentDepth == kFarPlaneDepth &&
        viewHeight < atmosphere.bottomRadius + atmosphere.skyViewLutMaxAltitude) {
        const vec3 upVector = worldPos / viewHeight;
        const float viewZenithCosAngle = dot(worldDir, upVector);

        // Rebuilds the frame the LUT was baked in, with forward along the sun's horizontal direction.
        vec3 sideVector = cross(upVector, worldDir);
        const float sideLength = length(sideVector);
        if (sideLength > 1e-6f) {
            sideVector /= sideLength;
        } else {
            // Straight up or down leaves the azimuth undefined; any frame will do, the LUT barely varies there.
            const vec3 fallbackAxis = abs(upVector.y) < 0.99f ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f);
            sideVector = normalize(cross(upVector, fallbackAxis));
        }
        const vec3 forwardVector = normalize(cross(sideVector, upVector));

        const vec2 lightOnPlane =
            vec2(dot(atmosphere.sunDirection, forwardVector), dot(atmosphere.sunDirection, sideVector));
        const float lightOnPlaneLength = length(lightOnPlane);

        // A sun at the exact zenith has no azimuth either, and the sky is rotationally symmetric then.
        const float lightViewCosAngle = lightOnPlaneLength > 1e-6f ? lightOnPlane.x / lightOnPlaneLength : 1.0f;

        const bool intersectsGround =
            raySphereIntersectNearest(worldPos, worldDir, vec3(0.0f), atmosphere.bottomRadius) >= 0.0f;
        const vec2 uv = skyViewLutParamsToUv(
            intersectsGround, viewZenithCosAngle, lightViewCosAngle, viewHeight, atmosphere.bottomRadius);

        L += textureLod(SkyViewLutTexture, uv, 0).rgb;
        finalColor = vec4(L * atmosphere.exposure, 1.0f);
        return;
    }

    // #endif

    // #if FASTAERIALPERSPECTIVE_ENABLED
    //     homogPos = atmosphere.invVP * vec4(ndcPos, fragmentDepth, 1.0f);
    //     const vec3 depthWorldPos = homogPos.xyz / homogPos.w;
    //
    //     // Figure out the depth slice that we need to sample our luminance from.
    //     const float tDepth = length(depthWorldPos.xyz - (worldPos + vec3(0.0f, -atmosphere.bottomRadius, 0.0f)));
    //     float slice = aerialPerspectiveDepthToSlice(tDepth);
    //     float weight = 1.0;
    //
    //     // For slice 0, we weigh everything down to 0 at depth 0.
    //     if (slice < 0.5)
    //     {
    //         weight = clamp(slice * 2.0, 0, 1);
    //         slice = 0.5;
    //     }
    //     float w = sqrt(slice / AP_SLICE_COUNT);  // squared distribution

    // finalColor = vec4(tDepth, slice, weight, w);

    //    const vec4 AP = weight * textureLod(AtmosphereCameraScatteringVolume, vec3(pixPos /
    //    vec2(atmosphere.screenResolution), w), 0); L.rgb += AP.rgb; float Opacity = AP.a;
    //
    //    finalColor = vec4(L, Opacity);

    // finalColor = vec4(L * 5, 1.0 - avgTransmittance);
    // utput.Luminance *= frac(clamp(w*AP_SLICE_COUNT, 0, AP_SLICE_COUNT));

    // #else // FASTAERIALPERSPECTIVE_ENABLED

    // Move to top atmosphere as the starting point for ray marching.
    // This is critical to be after the above to not disrupt above atmosphere tests and voxel selection.
    if (!moveToTopAtmosphere(worldPos, worldDir, atmosphere.topRadius)) {
        // Ray is not intersecting the atmosphere
        finalColor = vec4(
            getSunLuminance(atmosphere, worldPos, worldDir, angleToSun, sunEdgeFadeWidth) * atmosphere.exposure, 1.0f);
        return;
    }

    const bool ground = atmosphere.renderGround != 0;
    const float sampleCount = 0.0f;
    const bool variableSampleCount = true;
    const SingleScatteringResult ss = integrateScatteredLuminance(
        pixPos,
        worldPos,
        worldDir,
        atmosphere.sunDirection,
        atmosphere,
        ground,
        sampleCount,
        fragmentDepth,
        variableSampleCount,
        9000000.0f);

    L += ss.L;
    const float avgTransmittance = dot(ss.Transmittance, vec3(1.0f / 3.0f));
    finalColor = vec4(L * atmosphere.exposure, 1.0f - avgTransmittance);

    // #endif // FASTAERIALPERSPECTIVE_ENABLED
}
