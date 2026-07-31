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
    vec3 L;
    vec3 transmittance;
};

SingleScatteringResult integrateScatteredRadiance(
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
    result.transmittance = vec3(0.0f);

    vec3 clipSpace = vec3(pixPos / vec2(atmosphere.screenResolution) * 2.0f - 1.0f, kFarPlaneDepth);

    vec3 earthO = vec3(0.0f, 0.0f, 0.0f);
    float tBottom = raySphereIntersectNearest(worldPos, worldDir, earthO, atmosphere.bottomRadius);
    float tTop = raySphereIntersectNearest(worldPos, worldDir, earthO, atmosphere.topRadius);
    float tMax = 0.0f;
    if (tBottom < 0.0f) {
        if (tTop < 0.0f) {
            tMax = 0.0f;
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
            const vec4 homogDepthPos = atmosphere.invVP * vec4(clipSpace, 1.0);
            const vec3 depthWorldPos = homogDepthPos.xyz / (homogDepthPos.w * kMetersPerKilometer);

            // Undo the planet offset in worldPos to match; this engine is Y up.
            float tDepth = length(depthWorldPos - (worldPos + vec3(0.0, -atmosphere.bottomRadius, 0.0)));
            if (tDepth < tMax) {
                tMax = tDepth;
            }
        }
    }
    tMax = min(tMax, tMaxMax);

    float sampleCount = sampleCountIni;
    float sampleCountFloor = sampleCountIni;
    float tMaxFloor = tMax;
    if (variableSampleCount) {
        sampleCount = mix(atmosphere.minRayMarchingSamples, atmosphere.maxRayMarchingSamples, clamp(tMax * 0.01, 0, 1));
        sampleCountFloor = floor(sampleCount);
        tMaxFloor = tMax * sampleCountFloor / sampleCount; // rescale tMax to map to the last entire step segment.
    }
    float dt = tMax / sampleCount;

    const float cosTheta = dot(sunDir, worldDir);

    // Negated because worldDir is an incoming direction.
    const float miePhaseValue = computeMiePhaseFunction(atmosphere.miePhaseG, -cosTheta);
    const float rayleighPhaseValue = computeRayleighPhaseFunction(cosTheta);
    const vec3 globalL = atmosphere.sunIrradiance.rgb;

    vec3 L = vec3(0.0f);
    vec3 throughput = vec3(1.0);
    float t = 0.0f;
    const float sampleSegmentT = 0.3f;
    for (float s = 0.0f; s < sampleCount; s += 1.0f) {
        if (variableSampleCount) {
            // More expensive than a uniform step, but artifact free.
            float t0 = (s) / sampleCountFloor;
            float t1 = (s + 1.0f) / sampleCountFloor;
            t0 = t0 * t0;
            t1 = t1 * t1;
            t0 = tMaxFloor * t0;
            if (t1 > 1.0) {
                t1 = tMax;
            } else {
                t1 = tMaxFloor * t1;
            }
            t = t0 + (t1 - t0) * sampleSegmentT;
            dt = t1 - t0;
        } else {
            // Stepping by the exact difference rather than the nominal step keeps the integration accurate.
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
        // Energy conserving segment integration; Hillaire, Frostbite SIGGRAPH 2015, slide 28.
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

// The froxel volume holds the in-scattering and opacity already accumulated between the camera and each slice, so
// a shaded pixel needs one fetch instead of a march.
//
// sky-camera-volumes.frag stores layer i at ((i + 0.5) / N)^2 * N * kmPerSlice, a squared distribution that packs
// slices near the camera where the haze changes fastest. Inverting it for a distance t gives
// layer = sqrt(N * t / kmPerSlice) - 0.5.
//
// The volume is a 2D array rather than a 3D texture, so the sampler cannot filter across slices and the two
// neighbouring layers have to be blended by hand.
vec4 sampleAerialPerspective(const vec2 screenUv, const float tDepth) {
    float slice = tDepth / kCameraVolumeKmPerSlice;

    // The nearest slice is centred at a finite distance, but there is no haze at all at zero range, so fade the
    // whole contribution out over the first half slice rather than extrapolating below it.
    float weight = 1.0f;
    if (slice < 0.5f) {
        weight = clamp(slice * 2.0f, 0.0f, 1.0f);
        slice = 0.5f;
    }

    const float layer = sqrt(slice * kCameraVolumeLutSliceCount) - 0.5f;
    const float layer0 = clamp(floor(layer), 0.0f, kCameraVolumeLutSliceCount - 1.0f);
    const float layer1 = min(layer0 + 1.0f, kCameraVolumeLutSliceCount - 1.0f);

    const vec4 ap0 = textureLod(cameraVolumeLut, vec3(screenUv, layer0), 0);
    const vec4 ap1 = textureLod(cameraVolumeLut, vec3(screenUv, layer1), 0);
    return weight * mix(ap0, ap1, clamp(layer - layer0, 0.0f, 1.0f));
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
vec3 getSunRadiance(
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
    return atmosphere.sunDiskRadiance * coverage * computeSunLimbDarkening(centerToEdge);
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
    const vec3 targetWorldPos = homogPos.xyz / (homogPos.w * kMetersPerKilometer);
    vec3 worldDir = normalize(targetWorldPos - atmosphere.cameraPosition);

    vec3 worldPos = atmosphere.cameraPosition + vec3(0.0f, atmosphere.bottomRadius, 0.0f);
    const float viewHeight = length(worldPos);
    vec3 L = vec3(0.0f);

    // Must stay ahead of any branch that can diverge across a quad: fwidth is only defined in uniform control flow.
    const float angleToSun = acos(clamp(dot(worldDir, atmosphere.sunDirection), -1.0f, 1.0f));
    const float sunEdgeFadeWidth = max(fwidth(angleToSun), 1e-7f);

    // The one seam left: nothing writes viewDepthTexture yet, so every pixel reads as open sky and the aerial
    // perspective path below stays unreachable. A depth pre-pass turns this back into the texture fetch.
    const float fragmentDepth = kFarPlaneDepth; // textureLod(viewDepthTexture, screenUv, 0).r;

    if (fragmentDepth == kFarPlaneDepth) {
        L += getSunRadiance(atmosphere, worldPos, worldDir, angleToSun, sunEdgeFadeWidth);
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

    // Shaded geometry within reach of the froxel volume is resolved from it; the volume only spans
    // kCameraVolumeMaxDistance, so anything further away falls through to the full march.
    if (atmosphere.fastAerialPerspectiveEnabled != 0 && fragmentDepth != kFarPlaneDepth) {
        homogPos = atmosphere.invVP * vec4(ndcPos, fragmentDepth, 1.0f);
        const vec3 depthWorldPos = homogPos.xyz / (homogPos.w * kMetersPerKilometer);

        // Against cameraPosition rather than worldPos, which carries the planet offset.
        const float tDepth = length(depthWorldPos - atmosphere.cameraPosition);
        if (tDepth < kCameraVolumeMaxDistance) {
            const vec4 ap = sampleAerialPerspective(screenUv, tDepth);
            finalColor = vec4((L + ap.rgb) * atmosphere.exposure, ap.a);
            return;
        }
    }

    // Must stay after the fast sky path above, which tests against the unmoved camera position.
    if (!moveToTopAtmosphere(worldPos, worldDir, atmosphere.topRadius)) {
        finalColor = vec4(
            getSunRadiance(atmosphere, worldPos, worldDir, angleToSun, sunEdgeFadeWidth) * atmosphere.exposure, 1.0f);
        return;
    }

    const bool ground = atmosphere.renderGround != 0;
    const float sampleCount = 0.0f;
    const bool variableSampleCount = true;
    const SingleScatteringResult ss = integrateScatteredRadiance(
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
}
