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
layout(set = 1, binding = 2) uniform sampler2D skyViewLut;
layout(set = 1, binding = 3) uniform sampler2DArray cameraVolumeLut;
layout(set = 1, binding = 4) uniform sampler2D viewDepthTexture;

struct SingleScatteringResult {
    vec3 L;             // Scattered light (luminance)
    vec3 transmittance; // transmittance in [0,1] (unitless)
};

SingleScatteringResult integrateScatteredLuminance(
    in vec2 pixPos,
    in vec3 worldPos,
    in vec3 worldDir,
    in vec3 sunDir,
    in AtmosphereParams atmosphere,
    in bool ground,
    in float sampleCountIni,
    in float depthBufferValue,
    in bool variableSampleCount,
    float tMaxMax) {
    tMaxMax = 9000000.0f;
    SingleScatteringResult result;
    result.L = vec3(0.0f);
    result.transmittance = vec3(0.0f); // transmittance in [0,1] (unitless)

    vec3 clipSpace = vec3(pixPos / vec2(atmosphere.screenResolution) * 2.0f - 1.0f, kFarPlaneDepth);

    // Compute next intersection with atmosphere or ground
    vec3 earthO = vec3(0.0f, 0.0f, 0.0f);
    float tBottom = raySphereIntersectNearest(worldPos, worldDir, earthO, atmosphere.bottomRadius);
    float tTop = raySphereIntersectNearest(worldPos, worldDir, earthO, atmosphere.topRadius);
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

    if (depthBufferValue != kNoDepthBuffer) {
        clipSpace.z = depthBufferValue;
        if (clipSpace.z > kFarPlaneDepth) {
            vec4 depthBufferWorldPos = atmosphere.invVP * vec4(clipSpace, 1.0);
            depthBufferWorldPos /= depthBufferWorldPos.w;

            // Undo the planet offset in worldPos to match; this engine is Y up.
            float tDepth = length(depthBufferWorldPos.xyz - (worldPos + vec3(0.0, -atmosphere.bottomRadius, 0.0)));
            if (tDepth < tMax) {
                tMax = tDepth;
            }
        }
    }
    tMax = min(tMax, tMaxMax);

    // Sample count
    float sampleCount = sampleCountIni;
    float sampleCountFloor = sampleCountIni;
    float tMaxFloor = tMax;
    if (variableSampleCount) {
        sampleCount = mix(atmosphere.minRayMarchingSamples, atmosphere.maxRayMarchingSamples, clamp(tMax * 0.01, 0, 1));
        sampleCountFloor = floor(sampleCount);
        tMaxFloor = tMax * sampleCountFloor / sampleCount; // rescale tMax to map to the last entire step segment.
    }
    float dt = tMax / sampleCount;

    // Phase functions
    const float cosTheta = dot(sunDir, worldDir);
    const float miePhaseValue = computeMiePhaseFunction(atmosphere.miePhaseG, -cosTheta); // mnegate cosTheta because
                                                                                          // due to worldDir being a
                                                                                          // "in" direction.
    const float rayleighPhaseValue = computeRayleighPhaseFunction(cosTheta);
    const vec3 globalL = atmosphere.sunIlluminance.rgb;

    // Ray march the atmosphere to integrate optical depth
    vec3 L = vec3(0.0f);
    vec3 throughput = vec3(1.0);
    float t = 0.0f;
    const float sampleSegmentT = 0.3f;
    for (float s = 0.0f; s < sampleCount; s += 1.0f) {
        if (variableSampleCount) {
            // More expenssive but artefact free
            float t0 = (s) / sampleCountFloor;
            float t1 = (s + 1.0f) / sampleCountFloor;
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
            t = t0 + (t1 - t0) * sampleSegmentT;
            dt = t1 - t0;
        } else {
            // t = tMax * (s + sampleSegmentT) / sampleCount;
            //  Exact difference, important for accuracy of multiple scattering
            float newT = tMax * (s + sampleSegmentT) / sampleCount;
            dt = newT - t;
            t = newT;
        }
        const vec3 P = worldPos + t * worldDir;

        MediumSample medium = sampleMedium(P, atmosphere);
        const vec3 stepTransmittance = exp(-medium.extinction * dt);

        const float posHeight = length(P);
        const vec3 upVector = P / posHeight;
        const float sunZenithCosAngle = dot(sunDir, upVector);
        const vec3 transmittanceToSun =
            sampleTransmittanceLut(transmittanceLut, atmosphere, posHeight, sunZenithCosAngle);

        const vec3 phaseTimesScattering =
            medium.scatteringMie * miePhaseValue + medium.scatteringRay * rayleighPhaseValue;

        // Earth shadow
        const float tEarth =
            raySphereIntersectNearest(P, sunDir, earthO + kPlanetRadiusOffset * upVector, atmosphere.bottomRadius);
        const float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

        const vec3 multiScatteredL =
            atmosphere.multipleScatteringFactor *
            sampleMultipleScattering(multiScatteringLut, atmosphere, posHeight, sunZenithCosAngle);

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
        const vec3 P = worldPos + tBottom * worldDir;
        const float pHeight = length(P);
        const vec3 upVector = P / pHeight;
        const float sunZenithCosAngle = dot(sunDir, upVector);
        const vec3 transmittanceToSun = sampleTransmittanceLut(transmittanceLut, atmosphere, pHeight, sunZenithCosAngle);

        const float nDotL = clamp(sunZenithCosAngle, 0.0f, 1.0f);
        L += globalL * transmittanceToSun * throughput * nDotL * atmosphere.groundAlbedo / PI;
    }

    result.L = L;
    result.transmittance = throughput;
    return result;
}

float aerialPerspectiveDepthToSlice(float depth) {
    return depth * (1.0f / kCameraVolumeKmPerSlice);
}

float aerialPerspectiveSliceToDepth(float slice) {
    return slice * kCameraVolumeKmPerSlice;
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
        color.rgb = textureLod(skyViewLut, uv, 0).rgb * atmosphere.exposure;
        return true;
    case 4:
        // Sweeps the froxel slices horizontally so the whole volume is visible at once.
        color.rgb =
            textureLod(
                cameraVolumeLut,
                vec3(fract(uv * vec2(kCameraVolumeLutSliceCount, 1.0f)), floor(uv.x * kCameraVolumeLutSliceCount)),
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

    // No depth pre-pass writes viewDepthTexture yet, so every pixel is treated as open sky.
    const float fragmentDepth = kFarPlaneDepth; // textureLod(viewDepthTexture, screenUv, 0).r;

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

        L += textureLod(skyViewLut, uv, 0).rgb;
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
    //     float w = sqrt(slice / kCameraVolumeLutSliceCount);  // squared distribution

    // finalColor = vec4(tDepth, slice, weight, w);

    //    const vec4 AP = weight * textureLod(cameraVolumeLut, vec3(pixPos /
    //    vec2(atmosphere.screenResolution), w), 0); L.rgb += AP.rgb; float Opacity = AP.a;
    //
    //    finalColor = vec4(L, Opacity);

    // finalColor = vec4(L * 5, 1.0 - avgTransmittance);
    // output.Luminance *= frac(
    //     clamp(w * kCameraVolumeLutSliceCount, 0.0f, kCameraVolumeLutSliceCount));

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
    const float avgTransmittance = dot(ss.transmittance, vec3(1.0f / 3.0f));
    finalColor = vec4(L * atmosphere.exposure, 1.0f - avgTransmittance);

    // #endif // FASTAERIALPERSPECTIVE_ENABLED
}
