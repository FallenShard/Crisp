#ifndef CRISP_ATMOSPHERE_GLSL_H
#define CRISP_ATMOSPHERE_GLSL_H

struct AtmosphereParams {
    mat4 VP;
    mat4 invVP;

    // Rayleigh scattering coefficients + exponential distribution scale in the atmosphere
    vec3 rayleighScattering;
    float rayleighDensityExpScale;

    // Mie scattering coefficients + exponential distribution scale in the atmosphere
    vec3 mieScattering;
    float mieDensityExpScale;

    // Mie extinction coefficients + phase function excentricity
    vec3 mieExtinction;
    float miePhaseG;

    // Mie absorption coefficients
    vec4 mieAbsorption;

    // Ozone absorption and layer width
    vec3 ozoneAbsorption;

    // Another medium type in the atmosphere
    float absorptionDensity0LayerWidth;
    float absorptionDensity0ConstantTerm;
    float absorptionDensity0LinearTerm;
    float absorptionDensity1ConstantTerm;
    float absorptionDensity1LinearTerm;

    // The albedo of the ground.
    vec3 groundAlbedo;
    // Radius of the planet (center to ground) in km.
    float bottomRadius;

    // Direction from where the sun is facing
    vec3 sunDirection;
    // Maximum considered atmosphere height (center to atmosphere top) in km.
    float topRadius;

    vec4 sunIlluminance;

    vec3 cameraPosition;
    int minRayMarchingSamples;

    vec2 screenResolution;
    int maxRayMarchingSamples;
    int debugViewMode;

    // Multiplier applied to the ray marched luminance before it is written out.
    float exposure;

    // Angular diameter of the sun disk, in degrees.
    float sunAngularDiameterDegrees;

    // Luminance of the sun disk itself, in the same units as the ray marched sky.
    float sunDiskLuminance;

    // Artistic scale on the multiple scattering contribution; 1 is the physically motivated value.
    float multipleScatteringFactor;

    int drawSunDisk;

    // Sample the sky view LUT for open sky instead of ray marching it per pixel.
    int fastSkyEnabled;

    // Shade the planet surface where a view ray ends on it.
    int renderGround;

    // Altitude in km above which the sky view LUT is abandoned for a full march.
    float skyViewLutMaxAltitude;
};

struct MediumSample {
    vec3 scattering;
    vec3 absorption;
    vec3 extinction;

    vec3 scatteringMie;
    vec3 absorptionMie;
    vec3 extinctionMie;

    vec3 scatteringRay;
    vec3 absorptionRay;
    vec3 extinctionRay;

    vec3 scatteringOzo;
    vec3 absorptionOzo;
    vec3 extinctionOzo;

    vec3 albedo;
};

const float kPlanetRadiusOffset = 0.01f;

// The engine uses an infinite reverse-Z projection (Camera::reverseZPerspective): the near plane maps to 1 and
// infinity maps to 0.
const float kFarPlaneDepth = 0.0f;
const float kNoDepthBuffer = -1.0f;

// Must match the LUT constants in Models/Atmosphere.hpp.
const float kTransmittanceLutWidth = 256.0f;
const float kTransmittanceLutHeight = 64.0f;
const float kMultiScatteringLutResolution = 32.0f;
const float kSkyViewLutWidth = 192.0f;
const float kSkyViewLutHeight = 108.0f;
const float kCameraVolumeLutWidth = 32.0f;
const float kCameraVolumeLutHeight = 32.0f;
const float kCameraVolumeLutSliceCount = 32.0f;
const float kCameraVolumeKmPerSlice = 4.0f;

// Keeps bilinear taps from reaching past the edge of the LUT.
float fromUnitToSubUvs(float u, float resolution) {
    return (u + 0.5f / resolution) * (resolution / (resolution + 1.0f));
}

float fromSubUvsToUnit(float u, float resolution) {
    return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f));
}

vec3 getAlbedo3(vec3 scattering, vec3 extinction) {
    return scattering / max(vec3(0.001), extinction);
}

MediumSample sampleMedium(const vec3 worldPos, const AtmosphereParams atmosphere) {
    const float viewHeight = length(worldPos) - atmosphere.bottomRadius;

    const float densityMie = exp(atmosphere.mieDensityExpScale * viewHeight);
    const float densityRay = exp(atmosphere.rayleighDensityExpScale * viewHeight);
    const float densityOzo = clamp(
        viewHeight < atmosphere.absorptionDensity0LayerWidth
            ? atmosphere.absorptionDensity0LinearTerm * viewHeight + atmosphere.absorptionDensity0ConstantTerm
            : atmosphere.absorptionDensity1LinearTerm * viewHeight + atmosphere.absorptionDensity1ConstantTerm,
        0,
        1);

    MediumSample s;

    s.scatteringMie = densityMie * atmosphere.mieScattering;
    s.absorptionMie = densityMie * atmosphere.mieAbsorption.rgb;
    s.extinctionMie = densityMie * atmosphere.mieExtinction;

    s.scatteringRay = densityRay * atmosphere.rayleighScattering;
    s.absorptionRay = vec3(0.0f);
    s.extinctionRay = s.scatteringRay + s.absorptionRay;

    s.scatteringOzo = vec3(0.0f);
    s.absorptionOzo = densityOzo * atmosphere.ozoneAbsorption;
    s.extinctionOzo = s.scatteringOzo + s.absorptionOzo;

    s.scattering = s.scatteringMie + s.scatteringRay + s.scatteringOzo;
    s.absorption = s.absorptionMie + s.absorptionRay + s.absorptionOzo;
    s.extinction = s.extinctionMie + s.extinctionRay + s.extinctionOzo;
    s.albedo = getAlbedo3(s.scattering, s.extinction);

    return s;
}

float raySphereIntersectNearest(
    const vec3 rayOrigin, const vec3 rayDir, const vec3 sphereCenter, const float sphereRadius) {
    const float a = dot(rayDir, rayDir);
    const vec3 centerToOrigin = rayOrigin - sphereCenter;
    const float b = 2.0f * dot(rayDir, centerToOrigin);
    const float c = dot(centerToOrigin, centerToOrigin) - sphereRadius * sphereRadius;
    const float delta = b * b - 4.0f * a * c;
    if (delta < 0.0f || a == 0.0f) {
        return -1.0f;
    }

    float t1 = (-b - sqrt(delta)) / (2.0f * a);
    float t2 = (-b + sqrt(delta)) / (2.0f * a);
    if (t1 < 0.0f && t2 < 0.0f) {
        return -1.0f;
    }

    if (t1 < 0.0f) {
        return max(0.0f, t2);
    } else if (t2 < 0.0f) {
        return max(0.0f, t1);
    }

    return max(0.0f, min(t1, t2));
}

float intersectAtmosphere(const vec3 worldPos, const vec3 worldDir, const float bottomRadius, const float topRadius) {
    const vec3 earthOrigin = vec3(0.0f);
    const float tBottom = raySphereIntersectNearest(worldPos, worldDir, earthOrigin, bottomRadius);
    const float tTop = raySphereIntersectNearest(worldPos, worldDir, earthOrigin, topRadius);
    if (tBottom < 0.0f) {
        // No intersection occurs with either of the layers; we are outside of the atmosphere, looking towards space.
        if (tTop < 0.0f) {
            return -1.0f;
        }

        // Intersection occurs with just the outer layer; we are in the atmosphere, looking above the horizon.
        return tTop;
    }

    if (tTop < 0.0f) {
        return tBottom;
    }

    // We intersect both layers, pick the closer intersection; we look towards the ground.
    return min(tTop, tBottom);
}

bool moveToTopAtmosphere(inout vec3 worldPos, const in vec3 worldDir, const in float atmosphereTopRadius) {
    const float viewHeight = length(worldPos);

    // Check if we are above the atmosphere (in the space).
    if (viewHeight > atmosphereTopRadius) {
        const float tTop = raySphereIntersectNearest(worldPos, worldDir, vec3(0.0f, 0.0f, 0.0f), atmosphereTopRadius);
        if (tTop < 0.0f) {
            // Ray is not intersecting the atmosphere.
            return false;
        }

        const vec3 upVector = worldPos / viewHeight;
        worldPos = worldPos + tTop * worldDir - kPlanetRadiusOffset * upVector;
    }

    return true; // ok to start tracing.
}

void uvToTransmittanceLutParams(
    const float bottomRadius, const float topRadius, in vec2 uv, out float viewHeight, out float viewZenithCosAngle) {
    const float H = sqrt(topRadius * topRadius - bottomRadius * bottomRadius);
    const float rho = H * uv.y;
    viewHeight = sqrt(rho * rho + bottomRadius * bottomRadius);

    const float dMin = topRadius - viewHeight;
    const float dMax = rho + H;
    const float d = dMin + uv.x * (dMax - dMin);
    viewZenithCosAngle = d == 0.0 ? 1.0f : (H * H - rho * rho - d * d) / (2.0 * viewHeight * d);
    viewZenithCosAngle = clamp(viewZenithCosAngle, -1.0, 1.0);
}

// Inverse of uvToTransmittanceLutParams.
vec2 transmittanceLutParamsToUv(
    const AtmosphereParams atmosphere, const float viewHeight, const float viewZenithCosAngle) {
    const float horizon = sqrt(
        max(0.0f, atmosphere.topRadius * atmosphere.topRadius - atmosphere.bottomRadius * atmosphere.bottomRadius));
    const float rho = sqrt(max(0.0f, viewHeight * viewHeight - atmosphere.bottomRadius * atmosphere.bottomRadius));

    const float discriminant =
        viewHeight * viewHeight * (viewZenithCosAngle * viewZenithCosAngle - 1.0f) +
        atmosphere.topRadius * atmosphere.topRadius;
    const float d = max(0.0f, -viewHeight * viewZenithCosAngle + sqrt(max(0.0f, discriminant)));

    const float dMin = atmosphere.topRadius - viewHeight;
    const float dMax = rho + horizon;
    return vec2((d - dMin) / (dMax - dMin), rho / horizon);
}

// The samplers are passed in because each pass binds them at its own set and binding.
vec3 sampleTransmittanceLut(
    const sampler2D lut, const AtmosphereParams atmosphere, const float viewHeight, const float viewZenithCosAngle) {
    return textureLod(lut, transmittanceLutParamsToUv(atmosphere, viewHeight, viewZenithCosAngle), 0).rgb;
}

vec3 sampleMultipleScattering(
    const sampler2D lut, const AtmosphereParams atmosphere, const float viewHeight, const float viewZenithCosAngle) {
    vec2 uv = clamp(
        vec2(
            viewZenithCosAngle * 0.5f + 0.5f,
            (viewHeight - atmosphere.bottomRadius) / (atmosphere.topRadius - atmosphere.bottomRadius)),
        vec2(0.0f),
        vec2(1.0f));
    uv = vec2(
        fromUnitToSubUvs(uv.x, kMultiScatteringLutResolution), fromUnitToSubUvs(uv.y, kMultiScatteringLutResolution));

    return textureLod(lut, uv, 0).rgb;
}

// Placeholder until a shadow map is wired in.
float getShadow(const AtmosphereParams atmosphere, const vec3 worldPos) {
    return 1.0f;
}

// The vertical axis splits at the horizon and spreads quadratically away from it on both sides, so the horizon -
// where the sky changes fastest - gets the most texels and lands exactly on a texel boundary.
//
// This and skyViewLutParamsToUv are exact inverses and have to stay that way, hence side by side rather than one
// in the producer and one in the consumer.
void uvToSkyViewLutParams(
    in vec2 uv, in float bottomRadius, in float viewHeight, out float viewZenithCosAngle, out float lightViewCosAngle) {
    uv = vec2(fromSubUvsToUnit(uv.x, kSkyViewLutWidth), fromSubUvsToUnit(uv.y, kSkyViewLutHeight));

    const float horizonDist = sqrt(max(0.0f, viewHeight * viewHeight - bottomRadius * bottomRadius));
    const float horizonAngle = acos(clamp(horizonDist / viewHeight, -1.0f, 1.0f));
    const float zenithHorizonAngle = PI - horizonAngle;

    if (uv.y < 0.5f) {
        float coord = 2.0f * uv.y;
        coord = 1.0f - coord;
        coord *= coord;
        coord = 1.0f - coord;
        viewZenithCosAngle = cos(zenithHorizonAngle * coord);
    } else {
        float coord = 2.0f * uv.y - 1.0f;
        coord *= coord;
        viewZenithCosAngle = cos(zenithHorizonAngle + horizonAngle * coord);
    }

    const float coord = uv.x * uv.x;
    lightViewCosAngle = -(coord * 2.0f - 1.0f);
}

vec2 skyViewLutParamsToUv(
    const bool intersectsGround,
    const float viewZenithCosAngle,
    const float lightViewCosAngle,
    const float viewHeight,
    const float bottomRadius) {
    const float horizonDist = sqrt(max(0.0f, viewHeight * viewHeight - bottomRadius * bottomRadius));
    const float horizonAngle = acos(clamp(horizonDist / viewHeight, -1.0f, 1.0f));
    const float zenithHorizonAngle = PI - horizonAngle;
    const float viewZenithAngle = acos(clamp(viewZenithCosAngle, -1.0f, 1.0f));

    vec2 uv;
    if (!intersectsGround) {
        float coord = viewZenithAngle / zenithHorizonAngle;
        coord = 1.0f - coord;
        coord = sqrt(max(0.0f, coord));
        coord = 1.0f - coord;
        uv.y = coord * 0.5f;
    } else {
        float coord = (viewZenithAngle - zenithHorizonAngle) / horizonAngle;
        coord = sqrt(max(0.0f, coord));
        uv.y = coord * 0.5f + 0.5f;
    }

    uv.x = sqrt(max(0.0f, -lightViewCosAngle * 0.5f + 0.5f));

    return vec2(fromUnitToSubUvs(uv.x, kSkyViewLutWidth), fromUnitToSubUvs(uv.y, kSkyViewLutHeight));
}

// Cornette-Shanks phase function.
float computeMiePhaseFunction(float g, float cosTheta) {
    const float k = 3.0f / (8.0f * PI) * (1.0f - g * g) / (2.0f + g * g);
    return k * (1.0f + cosTheta * cosTheta) / pow(1.0f + g * g - 2.0f * g * -cosTheta, 1.5f);
}

float computeRayleighPhaseFunction(float cosTheta) {
    return 3.0f / (16.0f * PI) * (1.0f + cosTheta * cosTheta);
}

#endif // CRISP_ATMOSPHERE_GLSL_H
