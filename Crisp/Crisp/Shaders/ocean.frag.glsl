#version 450 core

#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec4 finalColor;

layout(location = 0) in vec3 eyePosition;
layout(location = 1) in vec2 oceanWorldXZ;

#include "Brdf/microfacet.part.glsl"
#include "Common/ocean-draw.part.glsl"
#include "Common/view.part.glsl"

layout(set = 0, binding = 1) uniform sampler2DArray packedHeightDispXMap;
layout(set = 0, binding = 2) uniform sampler2DArray packedDispZNormalXMap;
layout(set = 0, binding = 3) uniform sampler2DArray normalZMap;

layout(set = 1, binding = 0) uniform View {
    ViewParameters view;
};

layout(set = 1, binding = 1) uniform samplerCube irrMap;
layout(set = 1, binding = 2) uniform samplerCube refMap;
layout(set = 1, binding = 3) uniform sampler2D brdfLut;

const float kScatterFraction = 0.35f;

vec3 computeEnvSpecular(vec3 eyeN, vec3 eyeV, vec3 F0, float roughness, out vec3 irradiance) {
    const vec3 worldN = (view.invV * vec4(eyeN, 0.0f)).rgb;
    irradiance = texture(irrMap, worldN).rgb;

    const vec3 eyeR = reflect(-eyeV, eyeN);
    const vec3 worldR = (view.invV * vec4(eyeR, 0.0f)).rgb;
    const float maxReflectionLod = float(textureQueryLevels(refMap) - 1);
    const vec3 reflection = textureLod(refMap, worldR, roughness * maxReflectionLod).rgb;
    const float NdotV = max(dot(eyeN, eyeV), 0.0f);
    const vec2 brdf = texture(brdfLut, vec2(NdotV, roughness)).xy;

    return reflection * (F0 * brdf.x + brdf.y);
}

struct SurfaceFields {
    vec2 slope;
    float waveHeight;
    vec4 displacementGradient; // dDx/dx, dDx/dz, dDz/dx, dDz/dz.
    float unresolvedSlopeVariance;
};

SurfaceFields sampleCascades(const float pixelSpacing, const float vertexSpacing) {
    SurfaceFields fields;
    fields.slope = vec2(0.0f);
    fields.waveHeight = 0.0f;
    fields.displacementGradient = vec4(0.0f);
    fields.unresolvedSlopeVariance = 0.0f;

    const float fftSize = float(textureSize(packedHeightDispXMap, 0).x);
    const vec2 texelSize = vec2(1.0f / fftSize);

    for (int c = 0; c < OCEAN_CASCADE_COUNT; ++c) {
        const vec3 uv = oceanCascadeUv(oceanWorldXZ, cascadeSizes[c], fftSize, c);
        const float slopeWeight = oceanBandResolveWeight(cascadeWavelengths[c], pixelSpacing);

        fields.slope += slopeWeight * vec2(texture(packedDispZNormalXMap, uv).g, texture(normalZMap, uv).r);
        // Slope the footprint swallowed is not gone: scaling the field by w scales its variance by
        // w^2, and the missing remainder widens the specular lobe below instead.
        fields.unresolvedSlopeVariance += (1.0f - slopeWeight * slopeWeight) * cascadeSlopeVariances[c];

        // Must match the weight ocean.vert applied, or the Jacobian describes a surface that was
        // never displaced. Uniform across the draw: it is a function of push constants alone.
        const float dispWeight = oceanBandResolveWeight(cascadeWavelengths[c], vertexSpacing);
        if (dispWeight <= 0.0f) {
            continue;
        }

        fields.waveHeight += dispWeight * texture(packedHeightDispXMap, uv).r;

        const float invDerivativeSpan = 0.5f * fftSize / cascadeSizes[c] * dispWeight;
        const float dxLeft = texture(packedHeightDispXMap, uv - vec3(texelSize.x, 0.0f, 0.0f)).g;
        const float dxRight = texture(packedHeightDispXMap, uv + vec3(texelSize.x, 0.0f, 0.0f)).g;
        const float dxBack = texture(packedHeightDispXMap, uv - vec3(0.0f, texelSize.y, 0.0f)).g;
        const float dxFront = texture(packedHeightDispXMap, uv + vec3(0.0f, texelSize.y, 0.0f)).g;
        const float dzLeft = texture(packedDispZNormalXMap, uv - vec3(texelSize.x, 0.0f, 0.0f)).r;
        const float dzRight = texture(packedDispZNormalXMap, uv + vec3(texelSize.x, 0.0f, 0.0f)).r;
        const float dzBack = texture(packedDispZNormalXMap, uv - vec3(0.0f, texelSize.y, 0.0f)).r;
        const float dzFront = texture(packedDispZNormalXMap, uv + vec3(0.0f, texelSize.y, 0.0f)).r;

        fields.displacementGradient += invDerivativeSpan * vec4(
            dxRight - dxLeft, dxFront - dxBack, dzRight - dzLeft, dzFront - dzBack);
    }

    return fields;
}

void main() {
    const vec2 footprint = fwidth(oceanWorldXZ);
    const float pixelSpacing = max(footprint.x, footprint.y);
    const float vertexSpacing = patchWorldSize / float(gridSize);

    const SurfaceFields fields = sampleCascades(pixelSpacing, vertexSpacing);

    const float dDxDx = fields.displacementGradient.x;
    const float dDxDz = fields.displacementGradient.y;
    const float dDzDx = fields.displacementGradient.z;
    const float dDzDz = fields.displacementGradient.w;
    const float jacobianDeterminant =
        (1.0f - choppiness * dDxDx) * (1.0f - choppiness * dDzDz) - choppiness * choppiness * dDxDz * dDzDx;
    const float compression = max(1.0f - jacobianDeterminant, 0.0f);

    // Choppiness shears the tangents sideways; only the Jacobian terms carry that.
    const vec3 dPdx = vec3(1.0f - choppiness * dDxDx, fields.slope.x, -choppiness * dDzDx);
    const vec3 dPdz = vec3(-choppiness * dDxDz, fields.slope.y, 1.0f - choppiness * dDzDz);
    const vec3 worldN = normalize(cross(dPdz, dPdx));

    const vec3 eyeN = normalize((view.V * vec4(worldN, 0.0f)).xyz);
    const vec3 eyeV = normalize(-eyePosition);
    const float NdotV = max(dot(eyeN, eyeV), 0.0f);

    const vec3 eyeL = normalize((view.V * vec4(sunDirection, 0.0f)).xyz);
    const float NdotL = max(dot(eyeN, eyeL), 0.0f);

    const float crestFactor = smoothstep(0.0f, 1.5f, fields.waveHeight * invRmsWaveHeight);
    const float breakingFoam = smoothstep(foamThreshold, foamThreshold + foamSoftness, compression);
    const float foam = clamp(foamIntensity * breakingFoam * crestFactor, 0.0f, 1.0f);

    const float baseRoughness = mix(max(waterRoughness, 0.03f), 0.35f, foam);
    const float baseAlpha = baseRoughness * baseRoughness;
    // Toksvig/LEAN for GGX: convolving the NDF with the slope distribution that fell below the
    // footprint adds 2*sigma^2 to alpha^2. Without it the lost cascades come back as crawling
    // aliasing rather than as a wider lobe.
    const float alpha =
        min(sqrt(baseAlpha * baseAlpha + 2.0f * slopeVarianceScale * fields.unresolvedSlopeVariance), 1.0f);
    const float roughness = sqrt(alpha);

    const vec3 F0 = vec3(0.02f);
    const vec3 F = fresnelSchlick(NdotV, F0);

    const vec3 eyeH = normalize(eyeL + eyeV);
    const float NdotH = max(dot(eyeN, eyeH), 0.0f);
    const float D = distributionGGX(NdotH, alpha); // GGX takes alpha, not perceptual roughness.
    const float G = geometrySmith(NdotV, NdotL, roughness);
    const vec3 sunFresnel = fresnelSchlick(max(dot(eyeV, eyeH), 0.0f), F0);
    const vec3 sunSpecular = (D * G * sunFresnel / max(4.0f * NdotV * NdotL, 0.001f)) * NdotL * sunIntensity;

    vec3 irradiance;
    const vec3 reflection = computeEnvSpecular(eyeN, eyeV, F0, roughness, irradiance);

    // Water has no Lambertian albedo, so both scatter terms share one (1 - F).
    const vec3 scatterColor = pow(vec3(12, 120, 167) / 255.0f, vec3(2.2f));
    const vec3 subsurfaceColor = pow(vec3(20, 140, 130) / 255.0f, vec3(2.2f));
    // Sun terms need the diffuse 1/PI; the irradiance map already has it baked in.
    const float backLitFactor = pow(max(dot(eyeV, -eyeL), 0.0f), 4.0f);
    const vec3 body =
        (1.0f - F) * (scatterColor * irradiance * kScatterFraction +
                      subsurfaceColor * crestFactor * backLitFactor * sunIntensity / PI);

    const vec3 waterRadiance = body + reflection + sunSpecular;
    const vec3 foamColor = pow(vec3(235, 244, 238) / 255.0f, vec3(2.2f));
    const vec3 foamRadiance = foamColor * (irradiance + sunIntensity * NdotL / PI);
    finalColor = vec4(mix(waterRadiance, foamRadiance, foam), 1.0f);
}
